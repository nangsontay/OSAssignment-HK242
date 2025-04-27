
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>

int empty(struct queue_t* q)
{
	if (q == NULL)
		return 1;
	return (q->size == 0);
}

void enqueue(struct queue_t* q, struct pcb_t* proc)
{
	/* TODO: put a new process to queue [q] */
	if (q == NULL)
		return;
	if (q->size == MAX_QUEUE_SIZE)
		return;
	q->proc[q->size] = proc;
	q->size++;
}

struct pcb_t* dequeue(struct queue_t* q)
{
	/* TODO: return a pcb whose prioprity is the highest
	 * in the queue [q] and remember to remove it from q
	 * */
	if (q == NULL)
		return NULL;
	if (q->size == 0)
		return NULL;

	// the higher the priority, the lower the value
	int best_idx = 0;
	for (int i = 1; i < q->size; i++)
	{
#ifdef MLQ_SCHED
		// In MLQ mode, queue holds a single prio level => FIFO
		// But still use prio for tie-breaking if mixed
		if (q->proc[i]->prio < q->proc[best_idx]->prio)
		{
			best_idx = i;
		}
#else
		// Non-MLQ: choose process with highest (default) priority
		if (q->proc[i]->priority < q->proc[best_idx]->priority) {
			best_idx = i;
		}
#endif
	}
	struct pcb_t* proc = q->proc[best_idx];
	//shift the rest of the queue
	for (int i = best_idx; i < q->size - 1; i++)
	{
		q->proc[i] = q->proc[i + 1];
	}
	q->proc[--q->size] = NULL;
	return proc;
}
