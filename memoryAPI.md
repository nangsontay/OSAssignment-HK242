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
