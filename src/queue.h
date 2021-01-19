#ifndef __QUEUE_ROUND_ROBIN_H
#define __QUEUE_ROUND_ROBIN_H

#include "rr.h"

// Uma fila é uma lista de processos
typedef struct {
	processo* p_list;
	int capacity;
	int first;
	int last;
} fila;

fila* new_fila(int capacidade);
int incr_last(fila* queue);
int decr_last(fila* queue);
int incr_first(fila* queue);
bool is_empty(fila* queue);
bool is_full(fila* queue);
int push_back_processo(fila* queue, processo proc);
processo get_first_processo(fila* queue);
void rm_first_processo(fila* queue);
processo get_back_processo(fila* queue);
void rm_back_processo(fila* queue);
int move_processo(fila* leave, fila* enter);

#endif
