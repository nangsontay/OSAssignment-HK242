/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "common.h"
#include "syscall.h"
#include "stdio.h"
#include "libmem.h"

int __sys_killall(struct pcb_t *caller, struct sc_regs* regs)
{
    char proc_name[100];
    uint32_t data;
    int killed_count = 0;
    
    // Get the memory region ID from the system call argument
    uint32_t memrg = regs->a1;
    
    // Read the process name from the memory region
    int i = 0;
    do {
        if (libread(caller, memrg, i, &data) != 0) {
            return -1; // Memory read error
        }
        proc_name[i] = (char)data; //Stores the name in proc_name array
        i++;
    } while (data != -1 && i < 99);
    
    proc_name[i-1] = '\0'; // Adds null terminator for string operations
    
    if (i == 1 || i >= 99) {
        return -1; // Invalid process name
    }
    
    printf("Attempting to kill all processes named \"%s\"\n", proc_name);
    
    // Get the queues where processes might be
    struct queue_t *running = caller->running_list; // Contains currently running processes
    
#ifdef MLQ_SCHED
    struct queue_t *ready = caller->mlq_ready_queue; // Contains processes waiting to run
#else
    struct queue_t *ready = caller->ready_queue;    // Contains processes waiting to run
#endif
    
    // Check running processes
    if (running != NULL) {
        for (i = 0; i < running->size; i++) {
            struct pcb_t *proc = running->proc[i];
            if (proc != NULL && strcmp(proc->path, proc_name) == 0) { //Compares its name with the target
                // Mark process for termination
                proc->pc = proc->code->size; // If matched, sets the program counter (pc) to the end of the code segment
                killed_count++;
            }
        }
    }
    
    // Check ready queue
    if (ready != NULL) {
        for (i = 0; i < ready->size; i++) {
            struct pcb_t *proc = ready->proc[i];
            if (proc != NULL && strcmp(proc->path, proc_name) == 0) {
                // Mark process for termination
                proc->pc = proc->code->size; // Set PC to end of code
                killed_count++;
            }
        }
    }
    
    printf("Terminated %d process(es) named \"%s\"\n", killed_count, proc_name);
    return killed_count;
}
