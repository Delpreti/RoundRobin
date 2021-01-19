#ifndef __HEAP_ROUND_ROBIN_H
#define __HEAP_ROUND_ROBIN_H

#include <stdint.h>
#include "rr.h"

#define heap_front(h) (*(h)->data)
#define heap_free(h) (free((h)->data)), ((h)->data = (void*)0)

typedef processo heap_elem_t;

typedef struct st_heap_t {
	// Total de memória alocada para elementos
	uint32_t size;
	// Quantidade de elementos
	uint32_t count;
	// Elementos
	heap_elem_t *data;
} heap_t;

void heap_init( heap_t *heap );
void heap_push( heap_t *heap, heap_elem_t value );
heap_elem_t heap_pop( heap_t *heap );

void heapify( heap_elem_t data[], uint32_t count );

#endif
