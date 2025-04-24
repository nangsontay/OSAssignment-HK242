/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "string.h"
#include "mm.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
//NOTE: Only use mutex for touching mm struct, such as mm->mmap->vm_freerg_list, mm->symrgtbl, mm->fifo_pgn,etc.
static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;


int enlist_vm_freerg_list(struct mm_struct* mm, struct vm_rg_struct* rg_elmt)
{
  pthread_mutex_lock(&mmvm_lock);
  struct vm_rg_struct* rg_node = mm->mmap->vm_freerg_list;

  if (rg_elmt->rg_start >= rg_elmt->rg_end)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node;

  mm->mmap->vm_freerg_list = rg_elmt;
  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

struct vm_rg_struct* get_symrg_byid(struct mm_struct* mm, int rgid)
{
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

int __alloc(struct pcb_t* caller, int vmaid, int rgid, int size, int* alloc_addr)
{
  struct vm_rg_struct rgnode;
  //no need lock here
  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    pthread_mutex_lock(&mmvm_lock);
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
    *alloc_addr = rgnode.rg_start;
#ifdef VMDBG
    printf("===== PHYSICAL MEMORY AFTER ALLOCATION =====\n");
    printf("PID=%d - Region=%d - Address=%08d - Size=%d byte\n", caller->pid, rgid, *alloc_addr, size);
    print_pgtbl(caller, 0, -1);
#endif
    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }
  pthread_mutex_lock(&mmvm_lock);
  struct vm_area_struct* cur_vma = get_vma_by_num(caller->mm, vmaid);
  if (!cur_vma)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  pthread_mutex_unlock(&mmvm_lock);
  int inc_sz = PAGING_PAGE_ALIGNSZ(size);
  int old_sbrk = cur_vma->sbrk;

  struct sc_regs regs = {0};
  regs.a1 = SYSMEM_INC_OP; // 1) correct order
  regs.a2 = vmaid;
  regs.a3 = inc_sz;
  pthread_mutex_lock(&mmvm_lock);
  if (syscall(caller, 17, &regs) != 0)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  // cur_vma->vm_end += inc_sz;
  cur_vma->sbrk += inc_sz;

  // if (inc_sz > size)
  // {
  //   struct vm_rg_struct* freerg = malloc(sizeof *freerg);
  //   if (!freerg)
  //   {
  //     cur_vma->vm_end -= inc_sz;
  //     cur_vma->sbrk -= inc_sz;
  //     pthread_mutex_unlock(&mmvm_lock);
  //     return -1;
  //   }
  //   freerg->rg_start = old_sbrk + size;
  //   freerg->rg_end = old_sbrk + inc_sz;
  //   freerg->rg_next = NULL;
  //   pthread_mutex_unlock(&mmvm_lock);
  //   enlist_vm_freerg_list(caller->mm, freerg);
  //   pthread_mutex_lock(&mmvm_lock);
  // }

  // record region and return
  caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
  caller->mm->symrgtbl[rgid].rg_end = cur_vma->sbrk;
  *alloc_addr = old_sbrk;
  pthread_mutex_unlock(&mmvm_lock);
#ifdef VMDBG
  printf("===== PHYSICAL MEMORY AFTER ALLOCATION =====\n");
  printf("PID=%d - Region=%d - Address=%08d - Size=%d byte\n", caller->pid, rgid, *alloc_addr, size);
  print_pgtbl(caller, 0, -1);
#endif
  return 0;
}

int __free(struct pcb_t* caller, int vmaid, int rgid)
{
  pthread_mutex_lock(&mmvm_lock);
  struct vm_rg_struct* rgnode = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct* cur_vma = get_vma_by_num(caller->mm, vmaid);
  if (rgnode == NULL || cur_vma == NULL)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  struct vm_rg_struct* freerg = malloc(sizeof(struct vm_rg_struct));
  if (freerg == NULL)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  // freerg->rg_start = rgnode->rg_start;
  // freerg->rg_end = rgnode->rg_end;
  // freerg->rg_next = NULL;

  if (enlist_vm_freerg_list(caller->mm, rgnode) < 0)
  {
#ifdef VMDBG
    printf("===== PHYSICAL MEMORY DEALLOCATION FAILED =====\n");
#endif
    return -1;
  }
  // rgnode->rg_start = 0;
  // rgnode->rg_end = 0;
#ifdef VMDBG
  printf("===== PHYSICAL MEMORY AFTER DEALLOCATION =====\n");
  printf("PID=%d - Region=%d\n", caller->pid, rgid);
  print_pgtbl(caller, 0, -1);
#endif
  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

int liballoc(struct pcb_t* proc, uint32_t size, uint32_t reg_index)
{
  int addr, rc = __alloc(proc, 0, reg_index, size, &addr);
  if (rc == 0) proc->regs[reg_index] = addr; // 4)
  return rc;
}


int libfree(struct pcb_t* proc, uint32_t reg_index)
{
  return __free(proc, 0, reg_index);
}

int pg_getpage(struct mm_struct* mm, int pgn, int* fpn, struct pcb_t* caller)
{
  if (mm == NULL || mm->pgd == NULL || caller == NULL || fpn == NULL)
    return -1;
  uint32_t pte = mm->pgd[pgn];
  /* Page is not online, make it actively living */
  if (!PAGING_PAGE_PRESENT(pte))
  {
    /* Play with your paging theory here */
    int vicpgn, swpfpn, vicfpn;
    uint32_t vicpte;
    /* Find victim page */
    if (find_victim_page(caller->mm, &vicpgn) != 0)
    {
      printf("Error: Unable to find victim page\n");
      return -1;
    }
    /* Get free frame in MEMSWP */
    if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn) != 0)
    {
      printf("Error: Unable to get free frame in MEMSWP\n");
      return -1;
    }

    vicpte = caller->mm->pgd[vicpgn]; /*finding the victim page
                                                inside the page directory to get the
                                                page table entry, after that get the victim frame number*/
    vicfpn = PAGING_PTE_FPN(vicpte); //the victim frame storing our variable


    //syscall 17 memmap
    struct sc_regs regs;
    regs.a1 = SYSMEM_SWP_OP;
    regs.a2 = vicfpn;
    regs.a3 = swpfpn;
    if (syscall(caller, 17, &regs) != 0)
      return -1;

    int tmpfpn = PAGING_PTE_FPN(pte); //the target frame storing our variable
    //syscall 17 memmap copy target frame form swap to mem
    tmpfpn = PAGING_PTE_SWP(vicpte);
    regs.a1 = SYSMEM_SWP_OP;
    regs.a2 = tmpfpn;
    regs.a3 = vicfpn;
    if (syscall(caller, 17, &regs) != 0)
      return -1;

    /* Update page table */
    //Mark the victim page as being swapped out, swapped it into the swpfpn number frame in the SWAP
    pte_set_swap(&mm->pgd[vicpgn], 0, swpfpn);

    /* Update its online status of the target page */
    /* Mark the target page as being present in RAM (online), the target page is mapped into RAM
    at the frame vicfpn where the victim frame was before*/
    pte_set_fpn(&mm->pgd[pgn], vicfpn);

    enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
  }

  *fpn = PAGING_FPN(mm->pgd[pgn]);
  return 0;
}


/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to access
 *@value: value
 *
 */
int pg_getval(struct mm_struct* mm, int addr, BYTE* data, struct pcb_t* caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  int phyaddr = (fpn * PAGING_PAGESZ) + off;
  // printf("IM PHYSICAL ADDRESS %d\n", phyaddr);
  // MEMPHY_read(caller->mram, phyaddr, data);

  struct sc_regs* regs = malloc(sizeof(struct sc_regs));
  regs->a1 = SYSMEM_IO_READ;
  regs->a2 = phyaddr;
  if (syscall(caller, 17, regs) < 0)
  {
    return -1; //Syscall failed
  }
  *data = regs->a3;
  free(regs);
  return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct* mm, int addr, BYTE value, struct pcb_t* caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  int phyaddr = (fpn * PAGING_PAGESZ) + off;

  struct sc_regs* regs = malloc(sizeof(struct sc_regs));
  regs->a1 = SYSMEM_IO_WRITE;
  regs->a2 = phyaddr;
  regs->a3 = value;
  if (syscall(caller, 17, regs) < 0)
  {
    return -1; //Syscall failed
  }
  MEMPHY_write(caller->mram, phyaddr, value);

  // Update data
  // data = (BYTE)
  free(regs);
  return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t* caller, int vmaid, int rgid, int offset, BYTE* data)
{
  struct vm_rg_struct* currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct* cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;

  pg_getval(caller->mm, currg->rg_start + offset, data, caller);

  return 0;
}

int libread(struct pcb_t* proc, uint32_t source, uint32_t offset, uint32_t* destination)
{
  BYTE data;
  int val = __read(proc, 0, source, offset, &data);
  *destination = (uint32_t)data;

#ifdef IODUMP
  printf("===== PHYSICAL MEMORY AFTER READING =====\n");
  printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1);
#endif
  MEMPHY_dump(proc->mram);
#endif
  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __write(struct pcb_t* caller, int vmaid, int rgid, int offset, BYTE value)
{
  struct vm_rg_struct* currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct* cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;

  pg_setval(caller->mm, currg->rg_start + offset, value, caller);

  return 0;
}

/*libwrite - PAGING-based write a region memory */
int libwrite(
  struct pcb_t* proc, // Process executing the instruction
  BYTE data, // Data to be wrttien into memory
  uint32_t destination, // Index of destination register
  uint32_t offset)
{
  int val = __write(proc, 0, destination, offset, data);
#ifdef IODUMP
  printf("===== PHYSICAL MEMORY AFTER WRITING =====\n");
  printf("write region=%d offset=%d value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif
  return val;
}

/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t* caller)
{
  int pagenum, fpn;
  uint32_t pte;


  for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte = caller->mm->pgd[pagenum];

    if (!PAGING_PAGE_PRESENT(pte))
    {
      fpn = PAGING_PTE_FPN(pte);
      MEMPHY_put_freefp(caller->mram, fpn);
    }
    else
    {
      fpn = PAGING_PTE_SWP(pte);
      MEMPHY_put_freefp(caller->active_mswp, fpn);
    }
  }

  return 0;
}


/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct* mm, int* retpgn)
{
  pthread_mutex_lock(&mmvm_lock);
  struct pgn_t* pg = mm->fifo_pgn;
  if (pg == NULL)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  /* TODO: Implement the theorical mechanism to find the victim page */
  // FIFO so the retrieved page is the first one
  *retpgn = pg->pgn;
  mm->fifo_pgn = pg->pg_next;
  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}


/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t* caller, int vmaid, int size, struct vm_rg_struct* newrg)
{
  pthread_mutex_lock(&mmvm_lock);
  struct vm_area_struct* vma = get_vma_by_num(caller->mm, vmaid);
  if (!vma)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  struct vm_rg_struct *prev = NULL, *curr = vma->vm_freerg_list;
  while (curr)
  {
    unsigned long avail = curr->rg_end - curr->rg_start;
    if (avail >= size)
    {
      newrg->rg_start = curr->rg_start;
      newrg->rg_end = curr->rg_start + size;
      newrg->rg_next = NULL; // 2) init next

      curr->rg_start += size;
      if (curr->rg_start == curr->rg_end)
      {
        // remove curr
        if (prev) prev->rg_next = curr->rg_next;
        else vma->vm_freerg_list = curr->rg_next;
        free(curr);
      }
      pthread_mutex_unlock(&mmvm_lock);
      return 0;
    }
    prev = curr;
    curr = curr->rg_next;
  }
  pthread_mutex_unlock(&mmvm_lock);
  return -1;
}
