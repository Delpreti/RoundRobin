#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdbool.h>
#include <threads.h>
#include <time.h>

// Todo processo pode ser identificado por seu PID.
// Deixar como struct por enquanto, pode ser que o simulador precise guardar mais informacoes
typedef struct {
	pid_t pid;
	int priority; // Quanto menor, mais prioridade
} processo;

processo nulo;

// ----------- Implementacao de fila pode ficar em arquivo separado -------------------

// Uma fila eh uma lista de processos
typedef struct {
	processo* p_list;
	int capacity;
	int first;
	int last;
} fila;

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

// ------------------- Final do arquivo separado ---------------------------
// -------------------------------------------------------------------------

// Um estado contem uma (geralmente) ou mais filas
typedef struct {
	fila** f_list;
	int f_count;
	void (*fun_ptr)(void); // Ponteiro de funcao
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
estado* new_estado(int capacidade, void (*fun)(), int quant_filas){
	estado* state = malloc(sizeof(estado));
	state->f_count = quant_filas;
	state->f_list = malloc(quant_filas * sizeof(fila*));
	for(int i = 0; i < quant_filas; i++){
		state->f_list[i] = new_fila(capacidade);
	}
	state->fun_ptr = fun;
	sub_estado(state);
	return state;
}

fila* get_active_fila(estado *state){
	for(int i = 0; i < state->f_count; i++){
		if(!is_empty(state->f_list[i])){
			return state->f_list[i];
		}
	}
	return state->f_list[0]; // retorna a primeira por padrao mesmo que esteja vazia
}

// Funcao para inserir o processo no estado correto (revisar)
// Retorna 0 em caso de sucesso, 1 em caso de falha
int insert_proc(estado *state, processo proc){
	return push_back_processo(state->f_list[proc.priority], proc);
}

// Funcao generica que movimenta o proximo processo do estado A para o estado B (revisar)
void change_estado(estado* leave, estado* enter){
	if(move_processo(get_active_fila(leave), get_active_fila(enter)) == 0){
		// Cada estado tem uma funcao propria a ser chamada quando um processo novo eh adicionado
		enter->fun_ptr();
	}
}

estado* inicial;
estado* pronto;
// estado* suspenso; // Aguardando operacao de I/O
estado* execucao;
estado* finalizado;

// Funcao para criacao de um novo processo
// Nao precisa retornar o processo criado, pois todo processo vai direto para o estado inicial
void new_processo(char* nome, char* parametros){
	processo proc;
	proc.priority = 0; // Processo possui alta prioridade quando criado
	proc.pid = fork();
	if(proc.pid > 0){
		kill(proc.pid, SIGTSTP); // Pausa o processo filho
		push_back_processo(inicial->f_list[0], proc);
		// ACHO que nesse ponto o processo ja esta inicializado e ok, entao ja vou mandar pra fila de pronto
		change_estado(inicial, pronto);
	} else if(proc.pid == 0){
		sleep(1); // Garantir que o pai ira executar corretamente a pausa no processo filho
		// Quando o filho voltar, vai voltar daqui
		int check;	
		check = execl(nome, nome, parametros, (char *)NULL); // Deve ter que alterar isso eventualmente
		if(check == -1){
			//printf("Arquivo %s nao encontrado\n", nome);
			exit(1);
		}

	}
}

// Quando um processo terminar a execucao, ele ira sinalizar
void handle_child(int sinal){
	change_estado(execucao, finalizado); // Essa operacao nao pode falhar, senao da ruim
}

void faz_nada(){
	// literalmente
}

void executa(){
	kill(get_first_processo(execucao->f_list[0]).pid, SIGCONT);
}

void encerra(){
	// por enquanto nao precisa fazer nada, o processo fica parado na lista de finalizados
	// vai sair no final quando o programa chamar a limpeza
}

int busy(){
	return (execucao->f_list[0]->last == incr_first(execucao->f_list[0])) ? 1 : 0;
}

void initialize(){
	nulo.priority = -1;

	signal(SIGCHLD, handle_child);

	inicial = new_estado(30, faz_nada, 1);
	pronto = new_estado(50, faz_nada, 1); // Aumentar numero de filas (feedback)
	// suspenso = new_estado(50); // Aguardando operacao de I/O
	execucao = new_estado(1, executa, 1);
	finalizado = new_estado(30, encerra, 1);
}

int main(int argc, char** argv){

	initialize();
	// Puxar uma thread

	new_processo("teste", "4"); // Criar mais processos de teste

	// Colocar dentro de um loop, para ficar
	// sempre tentando colocar algum processo em execucao
	change_estado(pronto, execucao);

	int pid = get_back_processo(execucao->f_list[0]).pid;
	int status;
	while(busy()){}
	waitpid(pid, &status, 0);

	clean_estados();

    return 0;
}

// Definir entradas: processos a serem executados (e suas respectivas prioridades), numero de filas (feedback),
// numero de threads (?), Quantum/time slice

// Tempo de entrada do I/O aleatorio dentro do tempo de execucao do processo
// Duracao do I/O consistente (Disco mais rápido que impressora)

// Em execucao: Jogar processo na thread
// Timeout: Tirar processo da thread e jogar pro final da fila, chamar o proximo da fila


