# OS Sierra Memory Management APIs

This document provides a comprehensive overview of the memory management APIs in the OS Sierra project, focusing on the components and functions needed to complete the TODOs in the codebase.

## Table of Contents
1. [Memory Architecture Overview](#memory-architecture-overview)
2. [Key Data Structures](#key-data-structures)
3. [Memory Allocation APIs](#memory-allocation-apis)
4. [Memory Access APIs](#memory-access-apis)
5. [Page Replacement and Swapping](#page-replacement-and-swapping)
6. [Guide to Completing TODOs](#guide-to-completing-todos)

## Memory Architecture Overview

The OS Sierra implements a paging-based memory management system with the following components:

- **Physical Memory (RAM)**: Represented by `_ram[]` array in mem.c and accessed via `MEMPHY_*` functions
- **Swap Space**: Secondary storage for pages swapped out from RAM
- **Page Tables**: Maps virtual addresses to physical frames
- **Virtual Memory Areas (VMAs)**: Regions of virtual memory allocated to processes

The system uses a demand paging mechanism where pages are loaded into physical memory only when needed. When physical memory is full, victim pages are selected and swapped out to make room for new pages.

## Key Data Structures

### Process Control Block (pcb_t)
Each process has memory management information:
```c
struct pcb_t {
    // Other process data
    struct mm_struct *mm;       // Memory management information
    struct memphy_struct *mram; // Physical RAM
    struct memphy_struct *active_mswp; // Active swap space
    // Other fields
};
```

### Memory Management Structure (mm_struct)
Manages the virtual memory for a process:
```c
struct mm_struct {
    uint32_t *pgd;             // Page directory
    struct vm_area_struct *mmap; // List of VMAs
    struct pgn_t *fifo_pgn;    // FIFO page tracking for replacement
    struct vm_rg_struct symrgtbl[PAGING_MAX_SYMTBL_SZ]; // Symbol region table
};
```

### Virtual Memory Area (vm_area_struct)
Represents a contiguous region of virtual memory:
```c
struct vm_area_struct {
    int vm_id;                  // VMA ID
    int vm_start;               // Start address
    int vm_end;                 // End address
    int sbrk;                   // Current break pointer
    struct vm_rg_struct *vm_freerg_list; // Free regions
    struct vm_area_struct *vm_next;      // Next VMA
    struct mm_struct *vm_mm;    // Parent memory management
};
```

### Memory Region (vm_rg_struct)
Represents a specific region within a VMA:
```c
struct vm_rg_struct {
    int rg_start;               // Region start address
    int rg_end;                 // Region end address
    struct vm_rg_struct *rg_next; // Next region
};
```

### Physical Memory (memphy_struct)
Represents physical memory storage:
```c
struct memphy_struct {
    BYTE *storage;             // Actual storage
    int maxsz;                 // Maximum size
    struct framephy_struct *free_fp_list; // List of free frames
};
```

## Memory Allocation APIs

### High-Level Allocation Functions

#### 1. `liballoc(struct pcb_t* proc, uint32_t size, uint32_t reg_index)`
- User-facing function to allocate memory for a process
- Allocates a memory region of specified size
- Associates it with a region index in the symbol table
- Returns 0 on success

#### 2. `libfree(struct pcb_t* proc, uint32_t reg_index)`
- Frees a previously allocated memory region
- Identified by the region index in the symbol table
- Returns 0 on success

### Low-Level Allocation Functions

#### 1. `__alloc(struct pcb_t* caller, int vmaid, int rgid, int size, int* alloc_addr)`
- Allocates memory from a specific virtual memory area
- Handles the case when more memory is needed by extending the VMA
- Uses system call `sys_memmap` with `SYSMEM_INC_OP` operation

#### 2. `__free(struct pcb_t* caller, int vmaid, int rgid)`
- Frees a memory region and returns it to the free region list
- Updates the symbol region table

#### 3. `get_free_vmrg_area(struct pcb_t* caller, int vmaid, int size, struct vm_rg_struct* newrg)`
- Finds a free virtual memory region of specified size
- Populates the `newrg` structure with details of the allocated region
- Returns 0 on success, -1 if no suitable region is found

#### 4. `inc_vma_limit(struct pcb_t* caller, int vmaid, int inc_sz)`
- Increases the limit of a virtual memory area
- Called via system call when more memory is needed

## Memory Access APIs

### High-Level Access Functions

#### 1. `libread(struct pcb_t* proc, uint32_t source, uint32_t offset, uint32_t* destination)`
- Reads a byte from memory at region `source` with `offset`
- Stores the value in `destination`
- Handles page faults by loading pages if necessary

#### 2. `libwrite(struct pcb_t* proc, BYTE data, uint32_t destination, uint32_t offset)`
- Writes a byte `data` to memory at region `destination` with `offset`
- Handles page faults by loading pages if necessary

### Low-Level Access Functions

#### 1. `__read(struct pcb_t* caller, int vmaid, int rgid, int offset, BYTE* data)`
- Reads a byte from a specific memory region
- Translates the virtual address and handles page faults

#### 2. `__write(struct pcb_t* caller, int vmaid, int rgid, int offset, BYTE value)`
- Writes a byte to a specific memory region
- Translates the virtual address and handles page faults

#### 3. `pg_getval(struct mm_struct* mm, int addr, BYTE* data, struct pcb_t* caller)`
- Gets a value from a specific virtual address
- Ensures the page is in physical memory, swapping if necessary

#### 4. `pg_setval(struct mm_struct* mm, int addr, BYTE value, struct pcb_t* caller)`
- Sets a value at a specific virtual address
- Ensures the page is in physical memory, swapping if necessary

## Page Replacement and Swapping

### Page Management Functions

#### 1. `pg_getpage(struct mm_struct* mm, int pgn, int* fpn, struct pcb_t* caller)`
- Ensures a page is in physical memory
- If the page is not present, it triggers the page replacement algorithm
- Returns the frame page number (FPN) in physical memory

#### 2. `find_victim_page(struct mm_struct* mm, int* retpgn)`
- Implements the FIFO page replacement algorithm
- Identifies a victim page to be swapped out
- Currently implemented using a simple FIFO queue

### Page Table Management

#### 1. `pte_set_fpn(uint32_t *pte, int fpn)`
- Sets a page table entry for a page that is in physical memory
- Updates the presence bit and frame page number

#### 2. `pte_set_swap(uint32_t *pte, int swptyp, int swpoff)`
- Sets a page table entry for a page that has been swapped out
- Stores the swap type and offset

#### 3. `init_pte(uint32_t *pte, int pre, int fpn, int drt, int swp, int swptyp, int swpoff)`
- Initializes a page table entry with all necessary information

### Swapping Functions

#### 1. `__swap_cp_page(struct memphy_struct *mpsrc, int srcfpn, struct memphy_struct *mpdst, int dstfpn)`
- Copies the contents of a page from one physical location to another
- Used during swapping operations

#### 2. `__mm_swap_page(struct pcb_t* caller, int vicfpn, int swpfpn)`
- Handles the swapping of pages between RAM and swap space
- Called via system call from user space

## Guide to Completing TODOs

Several TODOs in the codebase need to be implemented. Here's a guide for each major area:

### 1. Memory Allocation (in `__alloc`)

In `libmem.c:73-114`, you need to:
- Set the vmaid for the region node
- Handle cases when `get_free_vmrg_area` fails by increasing the VMA limit
- Set up the system call registers for `sys_memmap` with `SYSMEM_INC_OP` operation
- Update the allocation address with the new memory region start address

```c
/* Example implementation */
// Set vmaid for the region node
rgnode.vmaid = vmaid;

// If get_free_vmrg_area fails, increase the VMA limit
if (get_free_vmrg_area(caller, vmaid, size, &rgnode) != 0) {
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
    int inc_sz = PAGING_PAGE_ALIGNSZ(size);
    int old_sbrk = cur_vma->sbrk;
    
    // Set up syscall registers
    struct sc_regs regs;
    regs.a1 = SYSMEM_INC_OP;
    regs.a2 = vmaid;
    regs.a3 = inc_sz;
    
    // Call sys_memmap to increase limit
    int inc_limit_ret = syscall(17, &regs);
    
    if (inc_limit_ret < 0)
        return -1;
    
    // Allocate from the newly expanded space
    *alloc_addr = old_sbrk;
    
    // Update symbol table
    caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
    caller->mm->symrgtbl[rgid].rg_end = old_sbrk + size;
    
    return 0;
}
```

### 2. Memory Freeing (in `__free`)

In `libmem.c:137-141`, you need to:
- Create a new region node from the symbol table entry
- Add the freed region to the free region list

```c
/* Example implementation */
struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));

// Initialize from the symbol table
rgnode->rg_start = caller->mm->symrgtbl[rgid].rg_start;
rgnode->rg_end = caller->mm->symrgtbl[rgid].rg_end;
rgnode->rg_next = NULL;

// Add to free region list
enlist_vm_freerg_list(caller->mm, rgnode);

// Clear the symbol table entry
caller->mm->symrgtbl[rgid].rg_start = caller->mm->symrgtbl[rgid].rg_end = 0;
```

### 3. Page Swapping (in `pg_getpage`)

In `libmem.c:194-238`, you need to:
- Handle the case when a page is not present in memory
- Set up syscall registers for swapping operations
- Update page table entries appropriately

```c
/* Example implementation */
// Get victim page information
uint32_t vicpte = mm->pgd[vicpgn];
int vicfpn = PAGING_FPN(vicpte);

// Set up syscall for swapping victim page to swap space
struct sc_regs regs;
regs.a1 = SYSMEM_SWP_OP;
regs.a2 = vicfpn;
regs.a3 = swpfpn;
syscall(17, &regs);

// Update victim page's PTE to indicate it's in swap space
pte_set_swap(&mm->pgd[vicpgn], PAGING_SWP, swpfpn);

// Now swap the target page from swap space to the freed frame
int tgtfpn = PAGING_SWP(pte);  // Get the swap location of our target
regs.a1 = SYSMEM_SWP_OP;
regs.a2 = tgtfpn;
regs.a3 = vicfpn;
syscall(17, &regs);

// Update target page's PTE to indicate it's now in memory
pte_set_fpn(&mm->pgd[pgn], vicfpn);
```

### 4. Memory Access (in `pg_getval` and `pg_setval`)

In `libmem.c:263-277` and `libmem.c:298-312`, you need to:
- Calculate the physical address from the frame number and offset
- Set up syscall registers for memory I/O operations
- Handle the returned data

```c
/* Example implementation for pg_getval */
int off = PAGING_OFFST(addr);
int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) | off;

// Set up syscall registers
struct sc_regs regs;
regs.a1 = SYSMEM_IO_READ;
regs.a2 = phyaddr;
regs.a3 = 0;  // Will be filled with read value

// Call sys_memmap for read operation
syscall(17, &regs);

// Update data with the read value
*data = (BYTE)regs.a3;
```

```c
/* Example implementation for pg_setval */
int off = PAGING_OFFST(addr);
int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) | off;

// Set up syscall registers
struct sc_regs regs;
regs.a1 = SYSMEM_IO_WRITE;
regs.a2 = phyaddr;
regs.a3 = value;

// Call sys_memmap for write operation
syscall(17, &regs);
```

### 5. Virtual Memory Mapping (in `vmap_page_range`)

In `mm.c:92-101`, you need to:
- Update the return region structure
- Map the range of frames to the address space

```c
/* Example implementation */
// Update return region
ret_rg->rg_start = addr;
ret_rg->rg_end = addr + pgnum * PAGING_PAGESZ;
ret_rg->vmaid = caller->mm->mmap->vm_id;

// Map frames to address space
struct framephy_struct *fpit = frames;
for (pgit = 0; pgit < pgnum && fpit != NULL; pgit++, fpit = fpit->fp_next) {
    pte_set_fpn(&caller->mm->pgd[pgn + pgit], fpit->fpn);
}
```

### 6. Frame Allocation (in `alloc_pages_range`)

In `mm.c:122-137`, you need to:
- Allocate frames from physical memory
- Build a linked list of allocated frames

```c
/* Example implementation */
*frm_lst = NULL;

for (pgit = 0; pgit < req_pgnum; pgit++) {
    if (MEMPHY_get_freefp(caller->mram, &fpn) == 0) {
        newfp_str = malloc(sizeof(struct framephy_struct));
        newfp_str->fpn = fpn;
        newfp_str->fp_next = *frm_lst;
        *frm_lst = newfp_str;
    } else {
        // Handle out of memory error
        return -3000;  // Use OOM error code
    }
}
```

## Conclusion

The memory management system in OS Sierra uses a paging mechanism with virtual memory areas and page replacement. To complete the TODOs, focus on:

1. Proper allocation and freeing of memory regions
2. Correct handling of page faults and swapping
3. Proper translation between virtual and physical addresses
4. Correct updating of page table entries

By implementing these components, you'll have a functional memory management system that handles allocation, access, and page replacement efficiently.
