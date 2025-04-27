
#include "queue.h"
#include "sched.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define lock_queue()  do { pthread_mutex_lock(&queue_lock); } while (0)
#define unlock_queue()  do { pthread_mutex_unlock(&queue_lock); } while (0)

static struct queue_t ready_queue;
static struct queue_t run_queue; //OBSOLETED
static pthread_mutex_t queue_lock;

static struct queue_t running_list;


#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
static int slot[MAX_PRIO];
static int curr_prio;
static int curr_slot;

#endif


int queue_empty(void)
{
#ifdef MLQ_SCHED
	lock_queue();
	for (int i = 0; i < MAX_PRIO; i++)
		if (!empty(&mlq_ready_queue[i]))
		{
			unlock_queue();
			return 0;
		}
	unlock_queue();
	return 1;
#else
	int result = empty(&ready_queue) && empty(&run_queue);
	unlock_queue();
	return result;
#endif
}

void init_scheduler(void)
{
	lock_queue();
#ifdef MLQ_SCHED
	for (int i = 0; i < MAX_PRIO; i++)
	{
		mlq_ready_queue[i].size = 0;
		slot[i] = MAX_PRIO - i;
	}
	unlock_queue();
	curr_prio = 0;
	curr_slot = MAX_PRIO;
#else
	ready_queue.size = 0;
	run_queue.size = 0;
	unlock_queue();
#endif
	running_list.size = 0;
}

#ifdef MLQ_SCHED
/*
 *  Stateful design for routine calling
 *  based on the priority and our MLQ policy
 *  We implement stateful here using transition technique
 *  State representation   prio = 0 .. MAX_PRIO, curr_slot = 0..(MAX_PRIO -
 * prio)
 */
struct pcb_t* get_mlq_proc(void)
{
	struct pcb_t* proc = NULL;
	/*TODO: get a process from PRIORITY [ready_queue].
	 * Remember to use lock to protect the queue.
	 * */
	if (queue_empty())
	{
		return NULL;
	}
	lock_queue();
	//if current priority still has time slots, use it
	if (curr_slot > 0 && !empty(&mlq_ready_queue[curr_prio]))
	{
		proc = dequeue(&mlq_ready_queue[curr_prio]);
		curr_slot--;
		unlock_queue();
		return proc;
	}
	//round-robin search for non-empty queue
	for (int offset = 1; offset < MAX_PRIO; offset++)
	{
		int p = (curr_prio + offset) % MAX_PRIO;
		if (!empty(&mlq_ready_queue[p]))
		{
			curr_prio = p;
			curr_slot = slot[p] - 1;
			proc = dequeue(&mlq_ready_queue[p]);
			unlock_queue();
			return proc;
		}
	}
	//if no process found, reset to priority 0
	curr_prio = 0;
	curr_slot = slot[0] - 1;
	proc = dequeue(&mlq_ready_queue[0]);
	unlock_queue();
	return proc;
}

void put_mlq_proc(struct pcb_t* proc)
{
	lock_queue();
	enqueue(&mlq_ready_queue[proc->prio], proc);
	unlock_queue();
}

void add_mlq_proc(struct pcb_t* proc)
{
	lock_queue();
	enqueue(&mlq_ready_queue[proc->prio], proc);
	unlock_queue();
}

struct pcb_t* get_proc(void)
{
	return get_mlq_proc();
}

void put_proc(struct pcb_t* proc)
{
	proc->ready_queue = &ready_queue;
	proc->mlq_ready_queue = mlq_ready_queue;
	proc->running_list = &running_list;
	put_mlq_proc(proc);
}

void add_proc(struct pcb_t* proc)
{
	proc->ready_queue = &ready_queue;
	proc->mlq_ready_queue = mlq_ready_queue;
	proc->running_list = &running_list;
	add_mlq_proc(proc);
}
#else
struct pcb_t *get_proc(void) {
	struct pcb_t *proc = NULL;
	/*TODO: get a process from [ready_queue].
	 * Remember to use lock to protect the queue.
	 * */
	lock_queue();
	if (empty(&ready_queue)) {
		unlock_queue();
		return NULL;
	}
	proc = dequeue(&ready_queue);
	unlock_queue();
	return proc;
}

void put_proc(struct pcb_t *proc) {
	lock_queue();
	enqueue(&run_queue, proc);
	unlock_queue();
}

void add_proc(struct pcb_t *proc) {
	lock_queue();
	enqueue(&ready_queue, proc);
	unlock_queue();
}
#endif
