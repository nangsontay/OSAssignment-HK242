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
#include "queue.h"
#include "string.h"

int __sys_killall(struct pcb_t *caller, struct sc_regs* regs) {
    char proc_name[100];
    uint32_t data;

    // Hardcode for demo only
    uint32_t memrg = regs->a1;

    // Initialize proc_name with zeros
    memset(proc_name, 0, sizeof(proc_name));

    // Read the process name from the memory region
    int index = 0;
    int result = 0;

    while (index < 99) {
        result = libread(caller, memrg, index, &data);
        
        if (result != 0) {
            break; // Exit on read error
        }

        // Check for termination conditions
        if (data == -1 || data == 0) {
            proc_name[index] = '\0'; // Null-terminate on end signal
            break;
        }

        // Store valid characters
        if (data > 0 && data < 128) {
            proc_name[index] = (char)data;
            index++;
        } else {
            proc_name[index] = '\0'; // Null-terminate on invalid character
            break;
        }
    }

    // Ensure null termination in case of full loop completion
    proc_name[index] = '\0';

    // Use a default name if no valid name was read
    if (index == 0) {
        strcpy(proc_name, "P0");  // Use a default name for testing
    }

    printf("Attempting to terminate processes named: \"%s\"\n", proc_name);

    //printf("The procname retrieved from memregionid %d is \"%s\"\n", memrg, proc_name);

    int terminated_count = 0;

    // Process the running list safely
    if (caller->running_list != NULL) {
        struct queue_t temp_queue;
        temp_queue.size = 0;

        while (!empty(caller->running_list)) {
            struct pcb_t *proc = dequeue(caller->running_list);
            if (proc == NULL) {
                break;
            }

            if (proc->path != NULL && strcmp(proc->path, proc_name) == 0) {
                printf("Terminating running process pid=%d, name=%s\n", proc->pid, proc->path);

                // Free memory regions with safety checks
                for (int j = 0; j < 10; j++) {
                    if (proc->regs[j] != 0) {
                        libfree(proc, proc->regs[j]);
                    }
                }

                terminated_count++;
            } else {
                enqueue(&temp_queue, proc);
            }
        }

        // Move processes back from temp queue to original queue
        while (!empty(&temp_queue)) {
            enqueue(caller->running_list, dequeue(&temp_queue));
        }
    }

    // Process the ready queue safely
    if (caller->ready_queue != NULL) {
        struct queue_t temp_queue;
        temp_queue.size = 0;

        while (!empty(caller->ready_queue)) {
            struct pcb_t *proc = dequeue(caller->ready_queue);
            if (proc == NULL) {
                break;
            }

            if (proc->path != NULL && strcmp(proc->path, proc_name) == 0) {
                printf("Terminating ready process pid=%d, name=%s\n", proc->pid, proc->path);

                // Free memory regions with safety checks
                for (int j = 0; j < 10; j++) {
                    if (proc->regs[j] != 0) {
                        libfree(proc, proc->regs[j]);
                    }
                }

                terminated_count++;
            } else {
                enqueue(&temp_queue, proc);
            }
        }

        // Move processes back from temp queue to original queue
        while (!empty(&temp_queue)) {
            enqueue(caller->ready_queue, dequeue(&temp_queue));
        }
    }

    printf("Total %d processes named \"%s\" terminated\n", terminated_count, proc_name);
    return terminated_count;
}
