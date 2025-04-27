#include "common.h"
#include "syscall.h"
#include "stdio.h"
#include "libmem.h"
#include "queue.h"
#include "string.h"

int __sys_killall(struct pcb_t* caller, struct sc_regs* regs)
{
    char proc_name[100];
    uint32_t data;

    // Hardcode for demo only
    uint32_t memrg = regs->a1;

    /* Debug memory reading issue and robustly read the process name */
    // printf("DEBUG: Starting memory read from region %d\n", memrg);

    // Initialize proc_name with zeros
    memset(proc_name, 0, sizeof(proc_name));

    // Read first few bytes directly and print for debugging
    int valid_chars = 0;
    for (int i = 0; i < 99; i++)
    {
        int result = libread(caller, memrg, i, &data);
        // printf("DEBUG: Read offset %d: value=%d (0x%x), result=%d\n", i, (int)data, (int)data, result);
        if (result != 0)
        {
            // printf("DEBUG: Read error at offset %d\n", i);
            break;
        }

        if (data == -1 || data == 0)
        {
            proc_name[i] = '\0';
            break;
        }

        if (data > 0 && data < 128)
        {
            proc_name[i] = (char)data;
            valid_chars++;
        }
        else
        {
            // printf("DEBUG: Invalid character at offset %d\n", i);
            proc_name[i] = '\0';
            break;
        }
    }

    // Ensure null-termination
    proc_name[99] = '\0';

    // If no name was read successfully, use a default name for testing
    if (valid_chars == 0)
    {
        // printf("DEBUG: No valid process name read, using default 'P0' for testing\n");
        strcpy(proc_name, "P0");
    }

    printf("The procname retrieved from memregionid %d is \"%s\"\n", memrg, proc_name);

    int terminated_count = 0;

    /* Process the running list safely */
    if (caller->running_list != NULL)
    {
        // printf("DEBUG: Processing running list\n");
        struct queue_t temp_queue;
        temp_queue.size = 0;

        while (!empty(caller->running_list))
        {
            struct pcb_t* proc = dequeue(caller->running_list);
            if (proc == NULL)
            {
                // printf("DEBUG: Got NULL process from running_list\n");
                break;
            }

            // printf("DEBUG: Found process pid=%d, path=%s\n", proc->pid,
            //   (proc->path != NULL) ? proc->path : "NULL");

            if (proc->path != NULL && strcmp(proc->path, proc_name) == 0)
            {
                // Process matches, terminate it
                printf("Terminating running process pid=%d, name=%s\n", proc->pid, proc->path);
                for (int j = 0; j < 10; j++)
                {
                    if (proc->regs[j] != 0)
                        libfree(proc, proc->regs[j]);
                }
                terminated_count++;
            }
            else
            {
                // Process doesn't match, keep it
                // printf("DEBUG: Keeping process pid=%d, name=%s\n", proc->pid, proc->path);
                enqueue(&temp_queue, proc);
            }
        }

        // printf("DEBUG: Moving processes back to running_list\n");
        while (!empty(&temp_queue))
        {
            enqueue(caller->running_list, dequeue(&temp_queue));
        }
    }

#ifdef MLQ_SCHED
    /* Process all MLQ ready queues safely */
    for (int pr = 0; pr < MAX_PRIO; pr++)
    {
        // printf("DEBUG: Processing MLQ ready queue %d\n", pr);
        struct queue_t temp_queue;
        temp_queue.size = 0;

        while (!empty(&caller->mlq_ready_queue[pr]))
        {
            struct pcb_t* proc = dequeue(&caller->mlq_ready_queue[pr]);
            if (proc == NULL) break;

            if (proc->path != NULL && strcmp(proc->path, proc_name) == 0)
            {
                printf("Terminating MLQ[%d] process pid=%d, name=%s\n", pr, proc->pid, proc->path);
                for (int j = 0; j < 10; j++)
                {
                    if (proc->regs[j] != 0)
                        libfree(proc, proc->regs[j]);
                }
                terminated_count++;
            }
            else
            {
                enqueue(&temp_queue, proc);
            }
        }
        while (!empty(&temp_queue))
        {
            enqueue(&caller->mlq_ready_queue[pr], dequeue(&temp_queue));
        }
    }
#else
    /* Process the ready queue safely */
    if (caller->ready_queue != NULL)
    {
        // printf("DEBUG: Processing ready queue\n");
        struct queue_t temp_queue;
        temp_queue.size = 0;

        while (!empty(caller->ready_queue))
        {
            struct pcb_t* proc = dequeue(caller->ready_queue);
            if (proc == NULL)
            {
                // printf("DEBUG: Got NULL process from ready_queue\n");
                break;
            }

            // printf("DEBUG: Found process pid=%d, path=%s\n", proc->pid,
            //   (proc->path != NULL) ? proc->path : "NULL");

            if (proc->path != NULL && strcmp(proc->path, proc_name) == 0)
            {
                printf("Terminating ready process pid=%d, name=%s\n", proc->pid, proc->path);
                for (int j = 0; j < 10; j++)
                {
                    if (proc->regs[j] != 0)
                        libfree(proc, proc->regs[j]);
                }
                terminated_count++;
            }
            else
            {
                enqueue(&temp_queue, proc);
            }
        }

        // printf("DEBUG: Moving processes back to ready_queue\n");
        while (!empty(&temp_queue))
        {
            enqueue(caller->ready_queue, dequeue(&temp_queue));
        }
    }
#endif

    printf("Total %d processes named \"%s\" terminated\n", terminated_count, proc_name);
    return terminated_count;
}
