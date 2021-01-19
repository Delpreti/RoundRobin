LDFLAGS = 
CFLAGS = -Wall
LIBFLAGS = 
CC = gcc
CFILES := $(shell find src/ -name '*.c' | sed -e 's/\.c/\.o/' | sed -e 's/src/obj/')
OBJS = ${CFILES}

all: mkd rr teste

mkd:
	mkdir -p obj

rr: ${OBJS}
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^ $(LIBFLAGS)

teste:
	$(CC) teste.c -o teste

obj/%.o : src/%.c
	$(CC) $(CFLAGS) -o $@ -c $< 

clean:
	rm -f obj/*.o rr teste
