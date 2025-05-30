#include "common.h"
#include "syscall.h"
#include "stdio.h"
#include "libmem.h"
#include "queue.h"
#include "string.h"
//

/* Memory region validation based on your system's memory management */
static int vaild_useraddr(struct pcb_t* proc, uint32_t addr, size_t size)
{
    // Basic checks to prevent integer overflow
    if (size == 0 || addr + size < addr)
    {
        return 0;
    }

    // Check if the memory region is registered for the process
    // This assumes your libread/libwrite functions already handle valid memory regions
    uint32_t test_data;
    if (libread(proc, addr, 0, &test_data) != 0)
    {
        return 0;
    }

    return 1;
}

static int copy_from_user(struct pcb_t* proc, void* kernel_dst, uint32_t user_src, size_t size)
{
    if (!vaild_useraddr(proc, user_src, size))
    {
        return -1;
    }

    uint32_t data;
    char* dst = (char*)kernel_dst;

    for (size_t i = 0; i < size; i++)
    {
        if (libread(proc, user_src, i, &data) != 0)
        {
            return -1;
        }
        dst[i] = (char)data;
    }
    return 0;
}

static int copy_to_user(struct pcb_t* proc, uint32_t user_dst, const void* kernel_src, size_t size)
{
    if (!vaild_useraddr(proc, user_dst, size))
    {
        return -1;
    }

    const char* src = (const char*)kernel_src;

    for (size_t i = 0; i < size; i++)
    {
        if (libwrite(proc, user_dst, i, (uint32_t)src[i]) != 0)
        {
            return -1;
        }
    }
    return 0;
}


//
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
    // int valid_chars = 0;

    if (copy_from_user(caller, proc_name, memrg, sizeof(proc_name) - 1) != 0)
    {
        printf("Error: Failed to copy process name from user space\n");
        return -1;
    }

    /* // NOT SAFE
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
    } */


    // Ensure null-termination
    proc_name[99] = '\0';

    // If no name was read successfully, use a default name for testing
    // if (valid_chars == 0)
    // {
    //     // printf("DEBUG: No valid process name read, using default 'P0' for testing\n");
    //     strcpy(proc_name, "P0");
    // }

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
