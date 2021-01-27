#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdbool.h>
#include <threads.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>

#include "heap.h"
#include "queue.h"
#include "rr.h"

#define TIMEOUT_MULTIPLIER_IO 0.1

extern processo nulo;
pthread_t io_thread;
heap_t io_heap;

// Clocks que demoram para cada tipo de IO
// 0 = DISCO		1s
// 1 = FITA			2s
// 2 = IMPRESSORA	5s
unsigned long long io_time[3] = {1000000, 2000000, 5000000 };

// Fila de prioridade para qual o processo vai depois do IO (1 baixa, 0 alta)
int io_fila[3] = {1, 0, 0};

char *get_io_name(int type){
	switch(type){
		case 0: return "Disco";
		case 1: return "Fita";
		case 2: return "Impressora";
		default: return "";
	}
}

clock_t io_time_control;

// Um estado contem uma (geralmente) ou mais filas
typedef struct {
	char* nome;
	fila** f_list;
	int f_count;
	void (*fun_ptr)(void); // Ponteiro de funcao
	int f_high;// Variavel que define qual a fila de alta prioridade
} estado;

// Mantemos uma lista de estados, para liberar memoria apos a execucao do simulador
// Todo estado deve ser adicionado a essa lista quando criado
estado* estados[5]; // deixa 5 por enquanto
int estados_count = 0;

// Funcao para guardar um estado na lista
void sub_estado(estado* state){
	estados[estados_count] = state;
	estados_count++;
}

// Funcao para realizar a limpeza a partir da lista de estados
void clean_estados(){
	for(int i = 0; i < estados_count; i++){
		for(int j = 0; j < estados[i]->f_count; j++){
			free(estados[i]->f_list[j]->p_list);
			free(estados[i]->f_list[j]);
		}
		free(estados[i]->f_list);
		free(estados[i]);
	}
}

// Inicializa um estado
estado* new_estado(char* name, int capacidade, void (*fun)(), int quant_filas){
	estado* state = malloc(sizeof(estado));
	state->f_high = 0;
	state->nome = name;
	state->f_count = quant_filas;
	state->f_list = malloc(quant_filas * sizeof(fila*));
	for(int i = 0; i < quant_filas; i++){
		state->f_list[i] = new_fila(capacidade);
	}
	state->fun_ptr = fun;
	sub_estado(state);
	return state;
}

estado* inicial;
estado* pronto;
estado* suspenso; // Aguardando operacao de I/O
estado* execucao;
estado* finalizado;

fila* get_enter_fila(estado *state, processo proc){
	return state == pronto ? state->f_list[proc.priority] : state->f_list[state->f_high];
}

fila* get_leave_fila(estado *state){
	return state->f_list[state->f_high]; // retorna sempre a fila de alta prioridade
}

char *get_time(){
	time_t momento = time(NULL);
	char* momento_format = strtok(ctime(&momento), " ");
	momento_format = strtok(NULL, " ");
	momento_format = strtok(NULL, " ");
	momento_format = strtok(NULL, " ");
	return momento_format;
}

// Funcao generica que movimenta um processo
// Retorna 0 em caso de sucesso, 1 em caso de falha
int move_processo_2(fila* leave, estado* enter){
	processo proc = get_first_processo(leave);
	if(push_back_processo(get_enter_fila(enter, proc), proc) == 0){
		rm_first_processo(leave);
		char* momento_format = get_time();
		printf("[%s] O processo %d mudou de estado: %s\n", momento_format, proc.pid, enter->nome);
		return 0;
	}
	return 1;
}

// Funcao generica que movimenta o proximo processo do estado A para o estado B (revisar)
void change_estado(estado* leave, estado* enter){
	fila* fila_out = get_leave_fila(leave);
	if(is_empty(fila_out)){
		leave->f_high = (leave->f_high + 1) % leave->f_count; // recuperacao de prioridade no feedback
		// Aumentar tambem a prioridade dos processos suspensos !!!
	}
	if(move_processo_2(get_leave_fila(leave), enter) == 0){
		// Cada estado tem uma funcao propria a ser chamada quando um processo novo eh adicionado
		enter->fun_ptr();
	}
}

void io_change_estado(estado *enter){
	heap_elem_t t = heap_pop(&io_heap);
	processo proc = t.proc;
	proc.priority = io_fila[t.io_type];
	if(push_back_processo(get_enter_fila(enter, proc), proc) == 0){
		char *momento_format = get_time();
		printf("[%s] O processo %d mudou de estado: %s\n", momento_format, proc.pid, enter->nome);
		enter->fun_ptr();
	}
}

// Funcao para criacao de um novo processo
// Nao precisa retornar o processo criado, pois todo processo vai direto para o estado inicial
void new_processo(char* nome, char* parametros){
	processo proc;
	proc.priority = pronto->f_high; // Processo possui alta prioridade quando criado
	proc.pid = fork();
	if(proc.pid > 0){
		push_back_processo(inicial->f_list[0], proc);
		// ACHO que nesse ponto o processo ja esta inicializado e ok, entao ja vou mandar pra fila de pronto
		change_estado(inicial, pronto);
	} else if(proc.pid == 0){
		raise(SIGTSTP);
		// Quando o filho voltar, vai voltar daqui
		int check;	
		check = execl(nome, nome, parametros, (char *)NULL); // Deve ter que alterar isso eventualmente
		if(check == -1){
			//printf("Arquivo %s nao encontrado\n", nome);
			exit(1);
		}

	}
}

// Quando um processo terminar a execucao, (ou apenas pausar) ele ira sinalizar
void handle_child(int sinal){
	int status;
	int quem = waitpid(0, &status, WNOHANG | WUNTRACED);
	//printf("Sinalizou: %d\n", quem);
	if(quem != 0 && WIFEXITED(status)){
		//printf("e entrou\n");
		change_estado(execucao, finalizado); // Essa operacao nao pode falhar, senao da ruim
	}
}

void prepare_io(){
	processo proc = get_first_processo(suspenso->f_list[0]);
	heap_elem_t t;
	t.proc = proc;
	t.io_type = rand()%3;
	t.io_end = io_time_control+io_time[t.io_type];
	heap_push(&io_heap, t);
	rm_first_processo(get_leave_fila(suspenso));
}

void faz_nada(){
	// literalmente
}

void chegada(){
	
}

void executa(){
	pid_t procpid = get_first_processo(execucao->f_list[0]).pid;
	kill(procpid, SIGCONT);
}

void encerra(){
	// por enquanto nao precisa fazer nada, o processo fica parado na lista de finalizados
	// vai sair no final quando o programa chamar a limpeza
}

int busy(){
	return get_back_processo(execucao->f_list[0]).priority != nulo.priority;
}

int userflag(char* flag){
	if(strcmp("-f", flag) == 0){
		return 0;
	}
	if(strcmp("-p1", flag) == 0){
		return 1;
	}
	if(strcmp("-p2", flag) == 0){
		return 2;
	}
	return -1;
}

void *io_check(){
	while(1){
		if(io_heap.count > 0 && io_time_control >= heap_front(&io_heap).io_end)
			io_change_estado(pronto);
		io_time_control = clock();
	}
	return NULL;
}

void initialize(){
	nulo.priority = -1;

	signal(SIGCHLD, handle_child);

	srand(time(NULL)); // Seed para geracao de tempos de I/O aleatorios
	heap_init(&io_heap);

	inicial = new_estado("Inicial", 30, faz_nada, 1);
	pronto = new_estado("Pronto", 50, chegada, 2); // Aumentar numero de filas (feedback)
	suspenso = new_estado("Suspenso (I/O)", 50, prepare_io, 1); // Aguardando operacao de I/O
	execucao = new_estado("Execucao", 1, executa, 1);
	finalizado = new_estado("Finalizado", 30, encerra, 1);
}

int main(int argc, char** argv){

	int timeout_time = 0;
	int quant_p1 = 1;
	int quant_p2 = 0;
	int i;

	void testflag(int f){
		switch(f){
			case 0:
				i++;
				timeout_time = strtol(argv[i], NULL, 10);
				break;
			case 1:
				i++;
				quant_p1 = strtol(argv[i], NULL, 10);
				break;
			case 2:
				i++;
				quant_p2 = strtol(argv[i], NULL, 10);
				break;
			default:
				// nao faz nada
				break;
		}
	}

	// pegar flags passadas pelo usuario no argv
	for(i = 0; i < argc; i++){
		testflag(userflag(argv[i]));
	}

	initialize();
	// Puxar uma thread (?)
	pthread_create(&io_thread, NULL, io_check, NULL);

	for(i = 0; i < quant_p1; i++){
		// Spawnar processos de tipo 1
		new_processo("teste", "4");
	}

	for(i = 0; i < quant_p2; i++){
		// Spawnar processos de tipo 2
		new_processo("teste", "10");
	}

	clock_t start;
	processo p_atual;
	int status;

	int processa(){
		double tick = 1;
		while(busy()){
			double elap = clock() - start;
			double elapsed_time = elap/CLOCKS_PER_SEC;
			// tenho a impressao que esse if deve virar um cnd_timedwait()
			if(timeout_time > 0 && elapsed_time >= timeout_time){
				kill(p_atual.pid, SIGTSTP);
				printf("Tempo limite de CPU excedido para o processo %d\n", p_atual.pid);
				p_atual.priority = (p_atual.priority + 1) % pronto->f_count;
				change_estado(execucao, pronto);
				return 1;
			}
			// aqui coloca um I/O aleatório
			if( elapsed_time >= tick*(timeout_time*TIMEOUT_MULTIPLIER_IO) ){
				if( rand()%10 == 0 ){
					kill(p_atual.pid, SIGTSTP);
					change_estado(execucao, suspenso);
					return 1;
				}
				++tick;
			}
		}
		return 0;
	}

	// Loop para executar os processos ate acabar
	while(!is_empty(get_leave_fila(pronto)) || io_heap.count > 0){
		change_estado(pronto, execucao);
		start = clock();
		p_atual = get_back_processo(execucao->f_list[0]);
		if(processa() == 0){
			waitpid(p_atual.pid, &status, 0);
		}
	}
	pthread_cancel(io_thread);
	clean_estados();

	return 0;
}

// Definir entradas: processos a serem executados (e suas respectivas prioridades), numero de filas (feedback),
// numero de threads (?), Quantum/time slice

// Tempo de entrada do I/O aleatorio dentro do tempo de execucao do processo
// Duracao do I/O consistente (Disco mais rápido que impressora)

// Em execucao: Jogar processo na thread
// Timeout: Tirar processo da thread e jogar pro final da fila, chamar o proximo da fila

