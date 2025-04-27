// #ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "libmem.h"

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
struct vm_area_struct* get_vma_by_num(struct mm_struct* mm, int vmaid)
{
  struct vm_area_struct* pvma = mm->mmap;

  if (mm->mmap == NULL)
    return NULL;

  int vmait = pvma->vm_id;

  while (vmait < vmaid)
  {
    if (pvma == NULL)
      return NULL;

    pvma = pvma->vm_next;
    vmait = pvma->vm_id;
  }

  return pvma;
}

int __mm_swap_page(struct pcb_t* caller, int vicfpn, int swpfpn)
{
  __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);
  return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */

struct vm_rg_struct* get_vm_area_node_at_brk(struct pcb_t* caller, int vmaid, int size, int alignedsz)
{
  struct vm_rg_struct* newrg = malloc(sizeof(struct vm_rg_struct));;
  struct vm_area_struct* cur_vma = get_vma_by_num(caller->mm, vmaid);
  if (cur_vma == NULL)
  {
    return NULL;
  }

  // Set the region boundaries based on the current break point
  newrg->rg_start = cur_vma->sbrk;
  newrg->rg_end = newrg->rg_start + alignedsz;
  newrg->rg_next = NULL;
  return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma end
 *@vmaend: vma end
 *
 *
 */
int validate_overlap_vm_area(struct pcb_t* caller, int vmaid, int vmastart, int vmaend)
{
  struct vm_area_struct* vma = caller->mm->mmap;

  // Iterate through all VMAs to check for overlaps
  while (vma != NULL)
  {
    // Skip checking against the same VMA we're validating
    if (vma->vm_id != vmaid)
    {
      // Check if a proposed region overlaps with existing VMA
      if (OVERLAP(vma->vm_start, vma->vm_end, vmastart, vmaend))
      {
        return -1; // Overlap detected
      }
    }
    vma = vma->vm_next;
  }
  return 0; // No overlap found
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size
 *
 */

int inc_vma_limit(struct pcb_t* caller, int vmaid, int inc_sz)
{
  // struct vm_rg_struct* newrg;
  int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
  int incnumpage = inc_amt / PAGING_PAGESZ;
  struct vm_rg_struct* area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt);
  struct vm_area_struct* cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (!area || !cur_vma)
  {
    free(area);
    printf("ERROR: Failed to get VM area node or current VMA\n"); //debug
    return -1;
  }

  int old_end = cur_vma->vm_end; //old_sbrk here

  /*Validate overlap of obtained region */
  if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0)
    return -1; /*Overlap and failed allocation */

  /* TODO: Obtain the new vm area based on vmaid */
  cur_vma->vm_end = area->rg_end;
  cur_vma->sbrk = area->rg_end;

  if (vm_map_ram(caller, area->rg_start, area->rg_end,
                 old_end, incnumpage, area) < 0)
  {
    cur_vma->vm_end = old_end;
    cur_vma->sbrk = old_end;
    free(area);
    printf("ERROR: Failed to map memory to RAM\n"); //debug
    return -1; /* Map the memory to MEMRAM */
  }
  free(area);
  return 0;
}

// int inc_vma_limit(struct pcb_t* caller, int vmaid, int inc_sz)
// {
//   struct vm_rg_struct* newrg = malloc(sizeof(struct vm_rg_struct));
//   int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
//   int incnumpage = inc_amt / PAGING_PAGESZ;
//   struct vm_area_struct* cur_vma = get_vma_by_num(caller->mm, vmaid);
//
//   if (cur_vma == NULL)
//   {
//     free(newrg);
//     return -1;
//   }
//
//   struct vm_rg_struct* area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt);
//   if (area == NULL)
//   {
//     free(newrg);
//     return -1;
//   }
//
//   int old_end = cur_vma->vm_end;
//
//   /*Validate overlap of obtained region */
//   if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0)
//   {
//     free(area);
//     free(newrg);
//     return -1; /*Overlap and failed allocation */
//   }
//
//   /* Update the VM area end point and break point */
//   cur_vma->vm_end = area->rg_end;
//
//   if (vm_map_ram(caller, area->rg_start, area->rg_end,
//                  old_end, incnumpage, newrg) < 0)
//   {
//     // Restore old values on failure
//     cur_vma->vm_end = old_end;
//     free(area);
//     free(newrg);
//     return -1; /* Map the memory to MEMRAM */
//   }
//   return 0;
// }

// #endif
