#include <stdlib.h>

#include "queue.h"
#include "rr.h"

processo* nulo;

// Uma fila deve ser inicializada com capacidade fixa, sem conter nenhum processo
fila* new_fila(int capacidade){
	fila* queue = malloc(sizeof(fila));
	queue->capacity = capacidade + 1;
	queue->first = 0;
	queue->last = 0;
	queue->p_list = malloc(capacidade * sizeof(processo*));
	return queue;
}

void clear_processo(processo* proc){
	free(proc->io_times);
	free(proc->io_types);
}

void clear_fila(fila* queue){
	if( !is_empty(queue) ){
		for(int i = queue->first; i != decr_last(queue); i = (i + 1) % queue->capacity){
			clear_processo(queue->p_list[i]);
			free(queue->p_list[i]);
		}
	}
	free(queue->p_list);
}

// Funcao para inserir o processo no final da fila
// Retorna 0 em caso de sucesso, 1 em caso de falha
int push_back_processo(fila* queue, processo* proc){
	if(is_full(queue) || (proc->priority == nulo->priority) ){
		return 1;
	}
	queue->p_list[queue->last] = proc;
	queue->last = incr_last(queue);
	return 0;
}

// Funcao que retorna o primeiro processo de uma fila
processo* get_first_processo(fila* queue){
	return is_empty(queue) ? nulo : queue->p_list[queue->first];
}

// Funcao para remover o primeiro processo de uma fila
void rm_first_processo(fila* queue){
	queue->p_list[queue->first] = nulo;
	if(!is_empty(queue)){
		queue->first = incr_first(queue);
	}
}

// Funcao que retorna o ultimo processo de uma fila
processo* get_back_processo(fila* queue){
	return is_empty(queue) ? nulo : queue->p_list[decr_last(queue)];
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

void incr_priorities(fila* queue, int f_count){
	if(is_empty(queue)){
		return;
	}
	for(int i = queue->first; i != decr_last(queue); i = (i + 1) % queue->capacity){
		if(queue->p_list[i]->priority != -1){
			queue->p_list[i]->priority = (queue->p_list[i]->priority + 1) % f_count;
		}
	}
}
