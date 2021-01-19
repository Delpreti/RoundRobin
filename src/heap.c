#include <stdlib.h>
#include <stdint.h>

#include "heap.h"

static const uint32_t BASE_SIZE = 32;

static inline int comparator( heap_elem_t lhs, heap_elem_t rhs ){
	return lhs.priority >= rhs.priority;
}

// Inicializa a estrutura da heap
void heap_init( heap_t *h ) {
	*h = (heap_t){
		.size = BASE_SIZE,
		.count = 0,
		.data = malloc( sizeof( heap_elem_t ) * BASE_SIZE )
	};
	if( !h->data ) exit(1);
}

// Coloca um elemento na heap
void heap_push( heap_t *h, heap_elem_t value ){
	uint32_t idx, parent;
	// Tenta realocar memória se a heap estiver sem espaço
	if( h->count == h->size ) {
		h->size <<= 1;
		h->data = realloc( h->data, sizeof( heap_elem_t ) * h->size );
		if( !h->data ) exit(1);
	}
	// Procura o lugar que o elemento pertence na heap
	for( idx = h->count++ ; idx ; idx = parent ) {
		parent = ( idx - 1 ) >> 1;
		if( comparator( h->data[parent], value ) ) break;
		h->data[idx] = h->data[parent];
	}
	h->data[idx] = value;
}

// Remove o elemento da heap
heap_elem_t heap_pop( heap_t *h ) {
	uint32_t idx, swp, other;
	heap_elem_t ret = heap_front(h);
	heap_elem_t temp = h->data[--h->count];
	// Tenta realocar a memória caso tenha tirado muitos elementos.
	if( ( h->count <= ( h->size >> 2 ) ) && ( h->size > BASE_SIZE ) ) {
		h->size >>= 1;
		h->data = realloc( h->data, sizeof( heap_elem_t ) * h->size );
		if( !h->data ) exit(1);
	}
	// Ajusta a heap após tirar o elemento
	for( idx = 0 ; 1 ; idx = swp ) {
		swp = ( idx << 1 ) + 1;
		if( swp >= h->count ) break;
		other = swp + 1;
		if( ( other < h->count ) && comparator( h->data[other], h->data[swp] ) ) swp = other;
		if( comparator( temp, h->data[swp] ) ) break;
		h->data[idx] = h->data[swp];
	}
	h->data[idx] = temp;
	return ret;
}

// Transforma uma array em uma heap
void heapify( heap_elem_t data[], uint32_t count ) {
	uint32_t item, idx, swp, other;
	heap_elem_t temp;
	item = ( count >> 1 ) - 1;
	while( 1 ) {
		// Acha a posição que o elemento precisa ter na subárvore
		temp = data[item];
		for( idx = item ; 1 ; idx = swp ) {
			// Acha o filho para trocar
			swp = ( idx << 1 ) + 1;
			if( swp >= count ) break;
			other = swp + 1;
			if( ( other < count ) && comparator( data[other], data[swp] ) ) swp = other;
			if( comparator( temp, data[swp] ) ) break;
			data[idx] = data[swp];
		}
		if( idx != item ) data[idx] = temp;
		if( !item ) return;
		--item;
	}
}
