# OS Sierra System Calls Documentation

This document provides a comprehensive description of all system calls implemented in the OS Sierra project.

## System Call Overview

| System Call Number | Name | Description |
|-------------------|------|-------------|
| 0 | listsyscall | Lists all available system calls |
| 17 | memmap | Memory management operations |
| 101 | killall | Terminates all processes with a specified name |

## Detailed Documentation

### 1. listsyscall (0)

**Purpose:**  
Displays a list of all available system calls in the system.

**Parameters:**  
- None (Caller PCB and register state are passed implicitly)

**Return Value:**  
- Returns 0 on success

**Implementation:**  
This system call iterates through the system call table and prints the name of each available system call. It is primarily used for debugging and informational purposes, allowing users to see what system calls are available.

**Usage Example:**  
```c
// Invoke system call 0 to list all available system calls
syscall(0);
```

### 2. memmap (17)

**Purpose:**  
Performs various memory-related operations, including mapping, incrementing virtual memory area limits, swapping pages, and direct memory I/O.

**Parameters:**  
- `a1`: Operation code (SYSMEM_MAP_OP, SYSMEM_INC_OP, SYSMEM_SWP_OP, SYSMEM_IO_READ, SYSMEM_IO_WRITE)
- `a2`: Address or other parameter depending on operation
- `a3`: Value or size parameter depending on operation

**Operation Codes:**
- `SYSMEM_MAP_OP`: Reserved for process mapping operations
- `SYSMEM_INC_OP`: Increases the virtual memory area limit
- `SYSMEM_SWP_OP`: Swaps pages in memory
- `SYSMEM_IO_READ`: Reads a byte from physical memory
- `SYSMEM_IO_WRITE`: Writes a byte to physical memory

**Return Value:**  
- Returns 0 on success
- For SYSMEM_IO_READ operations, the read value is placed in register a3

**Implementation:**  
This system call uses a switch-case structure to handle different memory operations. Each operation invokes appropriate memory management functions like `inc_vma_limit`, `__mm_swap_page`, `MEMPHY_read`, or `MEMPHY_write` based on the operation code.

**Usage Example:**  
```c
// Increment VMA limit
syscall(17, SYSMEM_INC_OP, vma_id, additional_size);

// Read from physical memory
uint32_t value;
syscall(17, SYSMEM_IO_READ, address, &value);
```

### 3. killall (101)

**Purpose:**  
Terminates all processes with a specified name.

**Parameters:**  
- `a1`: Memory region ID containing the target process name

**Return Value:**  
- Returns 0 on success

**Implementation:**  
This system call retrieves a process name from a specified memory region and terminates all processes matching that name. It reads characters from memory until encountering a -1 delimiter, constructs the process name, and then searches through process lists to terminate matching processes.

The current implementation includes TODOs for:
- Traversing the process list to find and terminate processes
- Matching the process name with processes in the running list
- Terminating all processes with the given name

**Usage Example:**  
```c
// Prepare memory region with process name "target_proc" followed by -1 delimiter
uint32_t mem_region = allocate_memory_region();
write_to_region(mem_region, "target_proc", -1);

// Call killall to terminate all processes named "target_proc"
syscall(101, mem_region);
```

## System Call Implementation Notes

System calls in OS Sierra follow a common structure:
1. Each system call is implemented as a function with prefix `__sys_` (e.g., `__sys_listsyscall`)
2. All system calls receive a pointer to the caller's PCB (`struct pcb_t *caller`) and register state (`struct sc_regs* regs`)
3. System calls access parameters through the register state structure (a1, a2, a3, etc.)
4. The system call table is defined in syscall.tbl and maps system call numbers to their implementation functions

This documentation covers the current implementation of system calls in OS Sierra as of April 2025.
