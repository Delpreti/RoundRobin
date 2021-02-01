#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdbool.h>
#include <threads.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>

#include "queue.h"
#include "rr.h"

#define TIMEOUT_MULTIPLIER_IO 0.1

extern processo* nulo;

#define MAX_JOBS 100
#define BINARY_TO_EXECUTE "teste"

#define IO_DEVICE_COUNT 3

pthread_t job_thread;

// Um estado contem uma (geralmente) ou mais filas
typedef struct {
	char* nome;
	fila** f_list;
	int f_count;
	void (*fun_ptr)(void); // Ponteiro de funcao
	int f_high; // Variavel que define qual a fila de alta prioridade
	pthread_t fun_thread; // Cada estado opera separadamente, com a propria thread
	int enabler;
	int completed;
} estado;

estado* inicial;
estado* pronto;
estado* suspenso_disco;
estado* suspenso_fita;
estado* suspenso_impressora;
estado* execucao;
estado* finalizado;

struct _job{
	int start_time;
	char duration[11];
}jobs[MAX_JOBS];
int job_idx, total_jobs;

pthread_mutex_t lock, weirdlock, weirderlock;
sem_t semaforo;

// Clocks que demoram para cada tipo de IO
// 0 = DISCO		1s
// 1 = FITA			2s
// 2 = IMPRESSORA	5s
int io_time[IO_DEVICE_COUNT] = {1, 2, 5}; // nao usado

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

void change_estado(estado* leave, estado* enter);

void *enable_fun(void* state_pointer){
	estado* state = (estado*) state_pointer;
	while(1){
		// Por padrao a funcao so executa uma vez, quando o processo eh inserido
		// Para alterar esse comportamento, pode-se setar o enabler para um valor diferente dentro da funcao chamada
		if(state->enabler > 0){
			state->fun_ptr();
			state->enabler -= 1;
			// printf("finalizei do %s\n", state->nome);
			if(state == pronto){
				sem_post(&semaforo);
			}
			//pthread_mutex_unlock(&state->proc_mutex);
		}
		if(state->completed > 0){
			//pthread_mutex_unlock(&lock);
			change_estado(state, pronto);
			state->completed--;
		}
	}
	return NULL;
}

// Mantemos uma lista de estados, para liberar memoria apos a execucao do simulador
// Todo estado deve ser adicionado a essa lista quando criado
estado* estados[7];
int estados_count = 0;
int p_encerrados;

// Funcao para guardar um estado na lista
void sub_estado(estado* state){
	estados[estados_count] = state;
	estados_count++;
}

// Funcao para realizar a limpeza a partir da lista de estados
void clean_estados(){
	for(int i = 0; i < estados_count; i++){
		while(estados[i]->enabler > 0){} // espera as threads terminarem
		for(int j = 0; j < estados[i]->f_count; j++){
			clear_fila(estados[i]->f_list[j]);
			free(estados[i]->f_list[j]);
		}
		pthread_cancel(estados[i]->fun_thread);
		free(estados[i]->f_list);
		free(estados[i]);
	}
}

// Inicializa um estado
estado* new_estado(char* name, int capacidade, void (*fun)(), int quant_filas){
	estado* state = malloc(sizeof(estado));
	state->f_high = 0;
	state->enabler = 0;
	state->completed = 0;
	// pthread_mutex_init(&state->proc_mutex, NULL);
	state->nome = name;
	state->f_count = quant_filas;
	state->f_list = malloc(quant_filas * sizeof(fila*));
	for(int i = 0; i < quant_filas; i++){
		state->f_list[i] = new_fila(capacidade);
	}
	state->fun_ptr = fun;
	sub_estado(state);
	pthread_create(&state->fun_thread, NULL, enable_fun, state);
	return state;
}

fila* get_enter_fila(estado *state, processo* proc){
	return state == pronto ? state->f_list[proc->priority] : state->f_list[state->f_high];
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
	processo* proc = get_first_processo(leave);
	//if(priority != -1) proc.priority = priority;
	pthread_mutex_lock(&weirderlock);
	if(push_back_processo(get_enter_fila(enter, proc), proc) == 0){
		rm_first_processo(leave);
		pthread_mutex_unlock(&weirderlock);
		char* momento_format = get_time();
		printf("[%s] O processo %d mudou de estado: %s\n", momento_format, proc->pid, enter->nome);
		return 0;
	}
	pthread_mutex_unlock(&weirderlock);
	return 1;
}

void adjust_priority(estado* state){
	state->f_high = (state->f_high + 1) % state->f_count; // recuperacao de prioridade no feedback
	// Quando isso ocorre, eh necessario ajustar tambem
	// a prioridade de todos os processos que estejam suspensos (antes que fujam)
	incr_priorities(suspenso_disco->f_list[0], pronto->f_count);
	incr_priorities(suspenso_fita->f_list[0], pronto->f_count);
	incr_priorities(suspenso_impressora->f_list[0], pronto->f_count);

	incr_priorities(execucao->f_list[0], pronto->f_count); // na teoria precisa tambem
}

// Funcao generica que movimenta o proximo processo do estado A para o estado B (fazer MUTEX aqui)
void change_estado(estado* leave, estado* enter){
	//while( pthread_mutex_trylock(&leave->proc_mutex) ){}
	//pthread_mutex_lock(&enter->proc_mutex);
	fila* fila_out = get_leave_fila(leave);
	pthread_mutex_lock(&lock);
	if(is_empty(fila_out)){
		adjust_priority(leave);
	}
	if(move_processo_2(fila_out, enter) == 0){
		// Cada estado tem uma funcao propria a ser chamada quando um processo novo eh adicionado
		enter->enabler += 1;
		if(enter == pronto){
			sem_wait(&semaforo);
		}
	}
	pthread_mutex_unlock(&lock);
}

// Funcao para criacao de um novo processo
// Nao precisa retornar o processo criado, pois todo processo vai direto para o estado inicial
void new_processo(char* nome, char* parametros){
	processo* proc = malloc(sizeof(processo));
	proc->priority = pronto->f_high; // Processo possui alta prioridade quando criado

	proc->io_count = rand() % 3; // Um unico processo pode solicitar um maximo de 3 IOs
	if(proc->io_count > 0){
		proc->io_times = malloc(proc->io_count * sizeof(int));
		proc->io_types = malloc(proc->io_count * sizeof(int));
		for(int i = 0; i < proc->io_count; i++){
			proc->io_times[i] = rand() % (strtol(parametros, NULL, 10) - 2) + 1;
			proc->io_types[i] = rand() % 3;
		}
	}

	proc->pid = fork();
	if(proc->pid > 0){
		push_back_processo(inicial->f_list[0], proc);
		// Nesse ponto o processo ja vai para a fila de pronto
		proc->arrive_time = time(NULL);
		change_estado(inicial, pronto);
	} else if(proc->pid == 0){
		raise(SIGTSTP);
		// Quando o filho voltar, vai voltar daqui
		int check;	
		check = execl(nome, nome, parametros, (char *)NULL);
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
	if(quem != 0 && WIFEXITED(status)){
		change_estado(execucao, finalizado); // Essa operacao nao pode falhar, senao da ruim
	}
}

#define not_empty(state) (!is_empty(get_leave_fila(state)))

int not_empty_pronto(){
	for(int i = 0; i < pronto->f_count; i++){
		if( !is_empty(pronto->f_list[i]) ){
			return 1; // true
		}
	}
	return 0; // false
}

int quant_p1;
int quant_p2;
int atraso;

void inicializacao(){
	for(int i = 0; i < quant_p1; i++){
		// Spawnar processos de tipo 1
		new_processo(BINARY_TO_EXECUTE, "4");
		// printf("entrei aqui\n");
		if(atraso > 0){
			//int conta = time(NULL);
			sleep(atraso);
			//printf("Atraso: %d, sleep: %d\n", atraso, (int) time(NULL) - conta);
		}
	}
	for(int i = 0; i < quant_p2; i++){
		// Spawnar processos de tipo 2
		new_processo(BINARY_TO_EXECUTE, "10");
		if(atraso > 0){
			sleep(atraso);
		}
	}
}

void chegada(){
	// Se o processo chega numa fila com prioridade ruim,
	// mas nao tem ninguem na fila prioritaria
	if( is_empty(get_leave_fila(pronto)) ){
		//pthread_mutex_lock(&lock);
		adjust_priority(pronto);
		//pthread_mutex_unlock(&lock);
	}
}

// Funcoes para I/O
void exec_io(estado* state, int time){
	if(not_empty(state)){
		sleep(time);
		state->completed++;
	}
}

void exec_disco(){
	exec_io(suspenso_disco, 1);
}

void exec_fita(){
	exec_io(suspenso_fita, 2);
}

void exec_impressora(){
	exec_io(suspenso_impressora, 5);
}

void executa(){
	pid_t procpid = get_first_processo(execucao->f_list[0])->pid;
	kill(procpid, SIGCONT);
}

void encerra(){
	processo* proc = get_back_processo(finalizado->f_list[0]);
	// Imprimir turnaround do processo
	int turnaround = time(NULL) - proc->arrive_time;
	printf("Turnaround do processo %d : %ds\n", proc->pid, turnaround);
	// printf("Pedidos de I/O do processo %d : %d\n", proc->pid, proc->io_count);

	p_encerrados++;
}

#define busy() ( get_back_processo(execucao->f_list[0])->priority != nulo->priority )

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
	if(strcmp("-i", flag) == 0){
		return 4;
	}
	return -1;
}

void initialize(){
	nulo = malloc(sizeof(processo));
	nulo->priority = -1;

	quant_p1 = 1;
	quant_p2 = 0;
	atraso = 0;

	signal(SIGCHLD, handle_child);

	srand(time(NULL)); // Seed para geracao de tempos de I/O aleatorios

	pthread_mutex_init(&lock, NULL);
	pthread_mutex_init(&weirdlock, NULL);
	pthread_mutex_init(&weirderlock, NULL);
	sem_init(&semaforo, 0, 0);

	inicial = new_estado("Inicial", 30, inicializacao, 1);

	pronto = new_estado("Pronto", 50, chegada, 2);

	suspenso_disco = new_estado("Suspenso / Disco", 15, exec_disco, 1);
	suspenso_fita = new_estado("Suspenso / Fita", 15, exec_fita, 1);
	suspenso_impressora = new_estado("Suspenso / Impressora", 15, exec_impressora, 1);

	execucao = new_estado("Execucao", 1, executa, 1);
	finalizado = new_estado("Finalizado", 30, encerra, 1);

	p_encerrados = 0;
}

int schedule_cmp(const void * a, const void * b){
	struct _job *A = (struct _job *)a;
	struct _job *B = (struct _job *)b;
	return (B->start_time - A->start_time);
}

void *schedule(void* useless_arg){
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
				// abre arquivo com configuração dos processos, ta um pouco bugado 
				// ele não imprime nada dos processos, também não sei se é só isso
				i++;
				config_file = fopen(argv[i], "r");
				break;
			case 4:
				i++;
				atraso = strtol(argv[i], NULL, 10);
				break;
			default:
				// deixar sempre vazio, para adicionar mais opcoes basta criar a flag na funcao userflag() e abrir um novo case
				break;
		}
	}

	initialize();

	// pegar flags passadas pelo usuario no argv
	for(i = 0; i < argc; i++){
		testflag(userflag(argv[i]));
	}

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
	} else {
		inicial->enabler += 1;
	}

	time_t start;
	processo* p_atual;
	int status;

	int processa(){
		while(busy()){
			int elapsed_time = time(NULL) - start;
			if(timeout_time > 0){
				if( elapsed_time >= timeout_time ){
					kill(p_atual->pid, SIGTSTP);
					printf("Tempo limite de CPU (%ds) excedido para o processo %d\n", elapsed_time, p_atual->pid);
					p_atual->priority = (p_atual->priority + 1) % pronto->f_count;
					change_estado(execucao, pronto);
					return 1;
				}
			}
			// Se precisar remover o IO, basta comentar esse for
			for(i = 0; i < p_atual->io_count; i++){
				if( elapsed_time == p_atual->io_times[i] ){
					pthread_mutex_lock(&weirdlock);
					p_atual->io_times[i] = -1;
					kill(p_atual->pid, SIGTSTP);
					printf("O processo %d solicitou I/O de %s\n", p_atual->pid, get_io_name(p_atual->io_types[i]) );
					p_atual->priority = (p_atual->priority + io_fila[p_atual->io_types[i]]) % pronto->f_count;
					change_estado(execucao, estados[ p_atual->io_types[i] + 2 ]);
					pthread_mutex_unlock(&weirdlock);
					return 1;
				}
			}
		}
		return 0;
	}

	// Loop para executar os processos ate acabar
	while( p_encerrados < (quant_p1 + quant_p2) ){
		if( is_empty(get_leave_fila(pronto)) ){
			continue;
		}
		change_estado(pronto, execucao);
		start = time(NULL);
		p_atual = get_back_processo(execucao->f_list[0]);
		if(processa() == 0){
			waitpid(p_atual->pid, &status, 0);
		}
	}
	
	// Limpeza
	// printf("double free clean\n");
	clean_estados();
	pthread_mutex_destroy(&lock);
	if(config_file != NULL){
		fclose(config_file);
		pthread_cancel(job_thread);
	}
	// printf("double free nulo\n");
	free(nulo);

	return 0;
}
