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

  int inc_sz = PAGING_PAGE_ALIGNSZ(size);
  int old_sbrk = cur_vma->sbrk;

  struct sc_regs regs = {0};
  regs.a1 = SYSMEM_INC_OP; // 1) correct order
  regs.a2 = vmaid;
  regs.a3 = inc_sz;
  if (syscall(caller, 17, &regs) != 0)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  cur_vma->vm_end += inc_sz;
  cur_vma->sbrk += inc_sz;

  if (inc_sz > size)
  {
    struct vm_rg_struct* freerg = malloc(sizeof *freerg);
    if (!freerg)
    {
      cur_vma->vm_end -= inc_sz;
      cur_vma->sbrk -= inc_sz;
      pthread_mutex_unlock(&mmvm_lock);
      return -1;
    }
    freerg->rg_start = old_sbrk + size;
    freerg->rg_end = old_sbrk + inc_sz;
    freerg->rg_next = NULL;
    pthread_mutex_unlock(&mmvm_lock);
    enlist_vm_freerg_list(caller->mm, freerg);
    pthread_mutex_lock(&mmvm_lock);
  }

  // record region and return
  caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
  caller->mm->symrgtbl[rgid].rg_end = old_sbrk + size;
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
  struct vm_rg_struct* region = get_symrg_byid(caller->mm, rgid);
  if (region == NULL || (region->rg_start == 0 && region->rg_end == 0))
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
  freerg->rg_start = region->rg_start;
  freerg->rg_end = region->rg_end;
  freerg->rg_next = NULL;

  enlist_vm_freerg_list(caller->mm, freerg);
  region->rg_start = 0;
  region->rg_end = 0;
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

  if (!PAGING_PAGE_PRESENT(pte))
  {
    int vicpgn, swpfpn, vicfpn, tgtfpn;

    if (find_victim_page(caller->mm, &vicpgn) != 0)
      return -1;

    vicfpn = PAGING_FPN(mm->pgd[vicpgn]);
    if (vicfpn < 0)
      return -1;

    if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn) != 0)
      return -1;

    tgtfpn = PAGING_PTE_SWP(pte);

    struct sc_regs regs;
    regs.a1 = SYSMEM_SWP_OP;
    regs.a2 = vicfpn;
    regs.a3 = swpfpn;
    if (syscall(caller, 17, &regs) != 0)
      return -1;

    regs.a2 = tgtfpn;
    regs.a3 = vicfpn;
    if (syscall(caller, 17, &regs) != 0)
      return -1;

    pte_set_swap(&mm->pgd[vicpgn], 0, swpfpn);
    pte_set_fpn(&mm->pgd[pgn], vicfpn);
    enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
  }

  *fpn = PAGING_FPN(mm->pgd[pgn]);
  return 0;
}

int pg_getval(struct mm_struct* mm, int addr, BYTE* data, struct pcb_t* caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1;

  int phyaddr = (fpn * PAGING_PAGESZ) + off;
  if (MEMPHY_read(caller->mram, phyaddr, data) != 0)
    return -1;

  return 0;
}

int pg_setval(struct mm_struct* mm, int addr, BYTE value, struct pcb_t* caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1;

  int phyaddr = (fpn * PAGING_PAGESZ) + off;
  if (MEMPHY_write(caller->mram, phyaddr, value) != 0)
    return -1;

  mm->pgd[pgn] |= (1U << 28); // Set dirty bit
  return 0;
}

int __read(struct pcb_t* caller, int vmaid, int rgid, int offset, BYTE* data)
{
  struct vm_rg_struct* currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct* cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL)
    return -1;

  return pg_getval(caller->mm, currg->rg_start + offset, data, caller);
}

int libread(struct pcb_t* proc, uint32_t source, uint32_t offset, uint32_t* destination)
{
  BYTE data;
  int val = __read(proc, 0, source, offset, &data);
  if (val < 0)
    return val;
  *destination = (uint32_t)data;

#ifdef IODUMP
  printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1);
#endif
  MEMPHY_dump(proc->mram);
#endif

  return val;
}

int __write(struct pcb_t* caller, int vmaid, int rgid, int offset, BYTE value)
{
  struct vm_rg_struct* currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct* cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL)
    return -1;

  return pg_setval(caller->mm, currg->rg_start + offset, value, caller);
}

int libwrite(struct pcb_t* proc, BYTE data, uint32_t destination, uint32_t offset)
{
#ifdef IODUMP
  printf("write region=%d offset=%d value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1);
#endif
  MEMPHY_dump(proc->mram);
#endif

  return __write(proc, 0, destination, offset, data);
}

int free_pcb_memph(struct pcb_t* caller)
{
  // 5) Reverse PRESENT check
  for (int pgn = 0; pgn < PAGING_MAX_PGN; pgn++)
  {
    uint32_t pte = caller->mm->pgd[pgn];
    if (PAGING_PAGE_PRESENT(pte))
    {
      int fpn = PAGING_PTE_FPN(pte);
      MEMPHY_put_freefp(caller->mram, fpn);
    }
    else if (pte & PAGING_PTE_SWAPPED_MASK)
    {
      int off = PAGING_PTE_SWP(pte);
      MEMPHY_put_freefp(caller->active_mswp, off);
    }
  }
  return 0;
}

int find_victim_page(struct mm_struct* mm, int* retpgn)
{
  pthread_mutex_lock(&mmvm_lock);
  struct pgn_t* pg = mm->fifo_pgn;
  if (pg == NULL)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  *retpgn = pg->pgn;
  mm->fifo_pgn = pg->pg_next;
  free(pg);
  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

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
