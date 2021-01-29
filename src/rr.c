#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdbool.h>
#include <threads.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>

#include "queue.h"
#include "rr.h"

#define TIMEOUT_MULTIPLIER_IO 0.1
#define MAX_JOBS 100
#define BINARY_TO_EXECUTE "teste"
extern processo nulo;

#define IO_DEVICE_COUNT 3
pthread_t io_thread, job_thread;

// Clocks que demoram para cada tipo de IO
// 0 = DISCO		1s
// 1 = FITA			2s
// 2 = IMPRESSORA	5s
long io_time[IO_DEVICE_COUNT] = {1, 2, 5};

// Fila de prioridade para qual o processo vai depois do IO (1 baixa, 0 alta)
int io_fila[IO_DEVICE_COUNT] = {1, 0, 0};

char *get_io_name(int type){
	switch(type){
		case 0: return "Disco";
		case 1: return "Fita";
		case 2: return "Impressora";
		default: return "";
	}
}

struct _job{
	int start_time;
	char duration[11];
}jobs[MAX_JOBS];
int job_idx, total_jobs;

// Um estado contem uma (geralmente) ou mais filas
typedef struct {
	char* nome;
	fila** f_list;
	int f_count;
	void (*fun_ptr)(void); // Ponteiro de funcao
	//int f_high; // Variavel que define qual a fila de alta prioridade
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
	//state->f_high = 0;
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
	return state == pronto ? state->f_list[proc.priority] : state->f_list[0];
}

fila* get_leave_fila(estado *state){
	return state->f_list[0]; // retorna sempre a fila de alta prioridade
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
int move_processo_2(fila* leave, estado* enter, int priority){
	processo proc = get_first_processo(leave);
	if(priority != -1) proc.priority = priority;
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
	//fila* fila_out = get_leave_fila(leave);
	if(move_processo_2(get_leave_fila(leave), enter, -1) == 0){
		// Cada estado tem uma funcao propria a ser chamada quando um processo novo eh adicionado
		enter->fun_ptr();
	}
}

void io_change_estado(estado *enter, int type){
	fila *leave = get_leave_fila(suspenso);
	if(move_processo_2(leave, enter, io_fila[type]) == 0){
		// rm_first_processo(leave);
		enter->fun_ptr();
	}
}

// Funcao para criacao de um novo processo
// Nao precisa retornar o processo criado, pois todo processo vai direto para o estado inicial
void new_processo(char* nome, char* parametros){
	processo proc;
	proc.priority = 0; // Processo possui alta prioridade quando criado
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
	// printf("Sinalizou: %d\n", quem);
	if(quem != 0 && WIFEXITED(status)){
		// printf("e entrou\n");
		change_estado(execucao, finalizado); // Essa operacao nao pode falhar, senao da ruim
	}
}

void prepare_io(){

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

#define busy() get_back_processo(execucao->f_list[0]).priority != nulo.priority

int userflag(char* flag){
	if(strcmp("-t", flag) == 0){
		return 0;
	}
	if(strcmp("-p1", flag) == 0){
		return 1;
	}
	if(strcmp("-p2", flag) == 0){
		return 2;
	}
	if(strcmp("-f", flag) == 0){
		return 3;
	}
	return -1;
}

void *io_check(){
	while(1){
		if( !is_empty(get_leave_fila(suspenso)) ){
			int type = rand() % IO_DEVICE_COUNT;
			pid_t procpid = get_first_processo(suspenso->f_list[0]).pid;
			if( procpid == 0 ) continue;
			printf("Processo %d vai executar o IO %s (%ld segundos).\n", procpid, get_io_name(type), io_time[type]);
			// Simulação do tempo de IO
			sleep(io_time[type]);
			io_change_estado(pronto, type);
		}
	}
	return NULL;
}

void initialize(){
	nulo.priority = -1;

	signal(SIGCHLD, handle_child);

	srand(time(NULL)); // Seed para geracao de tempos de I/O aleatorios

	inicial = new_estado("Inicial", 30, faz_nada, 1);
	pronto = new_estado("Pronto", 50, chegada, 2);
	suspenso = new_estado("Suspenso (I/O)", 50, prepare_io, 1); // Aguardando operacao de I/O
	execucao = new_estado("Execucao", 1, executa, 1);
	finalizado = new_estado("Finalizado", 30, encerra, 1);
}

int schedule_cmp(const void * a, const void * b){
	struct _job *A = (struct _job *)a;
	struct _job *B = (struct _job *)b;
	return (B->start_time - A->start_time);
}

void *schedule(){
	struct timeval start, last;
	gettimeofday(&start, NULL);
	while(job_idx < total_jobs){
		if( (last.tv_sec - start.tv_sec) >= jobs[job_idx].start_time ){
			new_processo(BINARY_TO_EXECUTE, jobs[job_idx].duration);
			++job_idx;
		}
		gettimeofday(&last, NULL);
	}
	return NULL;
}

int main(int argc, char** argv){

	int timeout_time = 0;
	int quant_p1 = 1;
	int quant_p2 = 0;
	int i;
	FILE *config_file = NULL;

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
			case 3:
				// abre arquivo com configuração dos processos, ta um pouco bugado (ele não imprime nada dos processos, também não sei se é só isso).
				// inicio duracao
				i++;
				config_file = fopen(argv[i], "r");
				break;
			default:
				// deixar sempre vazio, para adicionar mais opcoes basta criar a flag na funcao userflag() e abrir um novo case
				break;
		}
	}

	// pegar flags passadas pelo usuario no argv
	for(i = 0; i < argc; i++){
		testflag(userflag(argv[i]));
	}

	initialize();
	
	pthread_create(&io_thread, NULL, io_check, NULL);

	if(config_file){
		int inicio;
		char duracao[11];
		while(fscanf(config_file, " %d %s", &inicio, duracao) != EOF){
			jobs[total_jobs].start_time = inicio;
			strncpy(jobs[total_jobs].duration, duracao, 11);
			++total_jobs;
		}
		qsort(jobs, total_jobs, sizeof(struct _job), schedule_cmp);
		pthread_create(&job_thread, NULL, schedule, NULL);
	}else{
		for(i = 0; i < quant_p1; i++){
			// Spawnar processos de tipo 1
			new_processo(BINARY_TO_EXECUTE, "4");
		}

		for(i = 0; i < quant_p2; i++){
			// Spawnar processos de tipo 2
			new_processo(BINARY_TO_EXECUTE, "10");
		}
	}

	clock_t start;
	processo p_atual;
	int status;

	int processa(){
		double tick = 1;
		while(busy()){
			double elap = clock() - start;
			double elapsed_time = elap / CLOCKS_PER_SEC;
			// tenho a impressao que esse if deve virar um cnd_timedwait()
			if(timeout_time > 0 && elapsed_time >= timeout_time){
				kill(p_atual.pid, SIGTSTP);
				printf("Tempo limite de CPU excedido para o processo %d\n", p_atual.pid);
				p_atual.priority = 1;
				change_estado(execucao, pronto);
				return 1;
			}
			// aqui coloca um I/O aleatório, se precisar remover o IO só comentar esse if
			if( timeout_time > 0 && elapsed_time >= tick * (timeout_time * TIMEOUT_MULTIPLIER_IO) ){
				if( rand() % 15 == 0 ){
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
	while(!is_empty(get_leave_fila(pronto)) || !is_empty(get_leave_fila(suspenso)) || job_idx != total_jobs){
		if( !is_empty(get_leave_fila(pronto)) ){
			change_estado(pronto, execucao);
			start = clock();
			p_atual = get_back_processo(execucao->f_list[0]);
			if(processa() == 0){
				waitpid(p_atual.pid, &status, 0);
			}
		} else {
			move_processo(pronto->f_list[1], pronto->f_list[0]);
		}
	}

	// Limpeza
	pthread_cancel(io_thread);
	clean_estados();
	if(config_file) {
		fclose(config_file);
		pthread_cancel(job_thread);
	}

	return 0;
}
