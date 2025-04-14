
#include "queue.h"
#include "sched.h"
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;

#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
#endif

int queue_empty(void) {
#ifdef MLQ_SCHED
	unsigned long prio;
	for (prio = 0; prio < MAX_PRIO; prio++)
		if (!empty(&mlq_ready_queue[prio]))
			return -1;
#endif
	return (empty(&ready_queue) && empty(&run_queue));
}

void init_scheduler(void) {
#ifdef MLQ_SCHED
	int i;

	for (i = 0; i < MAX_PRIO; i++)
		mlq_ready_queue[i].size = 0;
#endif
	ready_queue.size = 0;
	run_queue.size = 0;
	pthread_mutex_init(&queue_lock, NULL);
}

#ifdef MLQ_SCHED
/*
 *  Stateful design for routine calling
 *  based on the priority and our MLQ policy
 *  We implement stateful here using transition technique
 *  State representation   prio = 0 .. MAX_PRIO, curr_slot = 0..(MAX_PRIO -
 * prio)
 */
struct pcb_t *get_mlq_proc(void) {
	struct pcb_t *proc = NULL;
	/*TODO: get a process from PRIORITY [ready_queue].
	 * Remember to use lock to protect the queue.
	 * */
    static int curr_prio = 0;
    static int curr_slot = MAX_PRIO;
	pthread_mutex_lock(&queue_lock);

    if (queue_empty() != -1) {
        pthread_mutex_unlock(&queue_lock);
        return NULL;
    }

    // get the process with greatest priority
    for (int i = 0; i < curr_prio; i++) {
        if (!empty(&mlq_ready_queue[i])) {
            curr_prio = i;
            curr_slot = MAX_PRIO - i - 1;
            proc = dequeue(&mlq_ready_queue[i]);

            pthread_mutex_unlock(&queue_lock);
            return proc;
        }
    }

    // if there is a process with the same priority and curr_slot > 0
    if (!empty(&mlq_ready_queue[curr_prio]) && curr_slot > 0) {
        proc = dequeue(&mlq_ready_queue[curr_prio]);
        curr_slot--;
        pthread_mutex_unlock(&queue_lock);
        return proc;
    }

    // slot went out, check for the next priority
    for (int i = curr_prio + 1; i < MAX_PRIO; i++) {
        if (!empty(&mlq_ready_queue[i])) {
            curr_prio = i;
            curr_slot = MAX_PRIO - i - 1;
            proc = dequeue(&mlq_ready_queue[i]);

            pthread_mutex_unlock(&queue_lock);
            return proc;
        }
    }

    // last resort, forced to get the process with the current priority
    if (!empty(&mlq_ready_queue[curr_prio])) {
        proc = dequeue(&mlq_ready_queue[curr_prio]);
    }

	pthread_mutex_unlock(&queue_lock);
	return proc;
}

void put_mlq_proc(struct pcb_t *proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t *proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);
}

struct pcb_t *get_proc(void) { 
	return get_mlq_proc(); 
	}

void put_proc(struct pcb_t *proc) { 
	return put_mlq_proc(proc); 
	}

void add_proc(struct pcb_t *proc) { 
	return add_mlq_proc(proc); 
	}
#else
struct pcb_t *get_proc(void) {
	struct pcb_t *proc = NULL;
	/*TODO: get a process from [ready_queue].
	 * Remember to use lock to protect the queue.
	 * */
	pthread_mutex_lock(&queue_lock);
	if (empty(&ready_queue)) {
		pthread_mutex_unlock(&queue_lock);
		return NULL;
	}
	proc = dequeue(&ready_queue);
	pthread_mutex_unlock(&queue_lock);
	return proc;
}

void put_proc(struct pcb_t *proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&run_queue, proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t *proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&ready_queue, proc);
	pthread_mutex_unlock(&queue_lock);
}
#endif
