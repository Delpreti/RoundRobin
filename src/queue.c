#include <stdlib.h>

#include "queue.h"
#include "rr.h"

processo nulo;

// Uma fila deve ser inicializada com capacidade fixa, sem conter nenhum processo
fila* new_fila(int capacidade){
	fila* queue = malloc(sizeof(fila));
	queue->capacity = capacidade + 1;
	queue->first = 0;
	queue->last = 0;
	queue->p_list = malloc(capacidade * sizeof(processo));
	return queue;
}

int incr_last(fila* queue){
	return (queue->last + 1) % queue->capacity;
}

int decr_last(fila* queue){
	return queue->last > 0 ? queue->last - 1 : queue->capacity - 1;
}

int incr_first(fila* queue){
	return (queue->first + 1) % queue->capacity;
}

bool is_empty(fila* queue){
	return queue->first == queue->last;
}

bool is_full(fila* queue){
	return incr_last(queue) == queue->first;
}

// Funcao para inserir o processo no final da fila
// Retorna 0 em caso de sucesso, 1 em caso de falha
int push_back_processo(fila* queue, processo proc){
	if(is_full(queue) || (proc.priority == nulo.priority) ){
		return 1;
	}
	queue->p_list[queue->last] = proc;
	queue->last = incr_last(queue);
	return 0;
}

// Funcao que retorna o primeiro processo de uma fila
processo get_first_processo(fila* queue){
	return is_empty(queue) ? nulo : queue->p_list[queue->first];
}

// Funcao para remover o primeiro processo de uma fila
void rm_first_processo(fila* queue){
	if(!is_empty(queue)){
		queue->first = incr_first(queue);
	}
}

// Funcao que retorna o ultimo processo de uma fila
processo get_back_processo(fila* queue){
	return is_empty(queue) ? nulo : queue->p_list[queue->last - 1];
}

// Funcao para remover o ultimo processo de uma fila
void rm_back_processo(fila* queue){
	if(!is_empty(queue)){
		queue->last = decr_last(queue);
	}
}

// Funcao generica que movimenta o proximo processo da fila A para a fila B
// Retorna 0 em caso de sucesso, 1 em caso de falha
int move_processo(fila* leave, fila* enter){
	if(push_back_processo(enter, get_first_processo(leave)) == 0){
		rm_first_processo(leave);
		return 0;
	}
	return 1;
}
