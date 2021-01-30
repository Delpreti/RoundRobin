#ifndef __QUEUE_ROUND_ROBIN_H
#define __QUEUE_ROUND_ROBIN_H

#include "rr.h"

// Uma fila é uma lista de processos
typedef struct {
	processo** p_list;
	int capacity;
	int first;
	int last;
} fila;

fila* new_fila(int capacidade);

#define incr_last(queue) ((queue->last + 1) % queue->capacity)
#define decr_last(queue) (queue->last > 0 ? queue->last - 1 : queue->capacity - 1)
#define incr_first(queue) ((queue->first + 1) % queue->capacity)
#define is_empty(queue) (queue->first == queue->last)
#define is_full(queue) (incr_last(queue) == queue->first)

void clear_processo(processo* proc);
void clear_fila(fila* queue);
int push_back_processo(fila* queue, processo* proc);
processo* get_first_processo(fila* queue);
void rm_first_processo(fila* queue);
processo* get_back_processo(fila* queue);
void rm_back_processo(fila* queue);
int move_processo(fila* leave, fila* enter);
void incr_priorities(fila* queue, int f_count);

#endif
