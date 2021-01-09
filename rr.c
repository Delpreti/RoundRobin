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
} processo;

// Todo estado tem uma lista com os processos que estao naquele estado
typedef struct {
	processo* p_list;
	int capacity;
	int first;
	int last;
	void (*fun_ptr)(void); // Ponteiro de funcao
} estado;

// Mantemos uma lista de estados, para liberar memoria apos a execucao do simulador
// Todo estado deve ser adicionado a essa lista quando criado
estado* estados[5]; // 5 eh a quantidade de estados que tem no nosso programa (corrigir/confirmar depois!)
int estados_count = 0;

// Funcao para guardar um estado na lista
void sub_estado(estado* state){
	estados[estados_count] = state;
	estados_count++;
}

// Funcao para realizar a limpeza a partir da lista de estados
void clean_estados(){
	for(int i = 0; i < estados_count; i++){
		/*for(int j = 0; j < estados[i]->current; j++){
			free(estados[i]->p_list[j].path);
		}*/
		free(estados[i]->p_list);
		free(estados[i]);
	}
}

// Um estado deve ser inicializado com capacidade fixa, sem conter nenhum processo
estado* new_estado(int capacidade, void (*fun)()){
	estado* state = malloc(sizeof(estado));
	state->capacity = capacidade + 1;
	state->first = 0;
	state->last = 0;
	state->p_list = malloc(capacidade * sizeof(processo));

	state->fun_ptr = fun;
	sub_estado(state);
	return state;
}

int incr_last(estado* state){
	return (state->last + 1) % state->capacity;
}

int decr_last(estado* state){
	return state->last > 0 ? state->last - 1 : state->capacity - 1;
}

int incr_first(estado* state){
	return (state->first + 1) % state->capacity;
}

// Funcao para inserir o processo no estado no final da lista
// Retorna 0 em caso de sucesso, 1 em caso de falha
int push_back_processo(estado* state, processo proc){
	if(incr_last(state) == state->first){
		return 1;
	}
	state->p_list[state->last] = proc;
	state->last = incr_last(state);
	return 0;
}

// Funcao que retorna o primeiro processo de um estado
processo get_first_processo(estado* state){
	return state->p_list[state->first];
}

// Funcao para remover o primeiro processo de um estado
void rm_first_processo(estado* state){
	if(state->last != state->first){
		state->first = incr_first(state);
	}
}

// Funcao que retorna o ultimo processo de um estado
processo get_back_processo(estado* state){
	return state->p_list[state->last - 1];
}

// Funcao para remover o ultimo processo de um estado
void rm_back_processo(estado* state){
	if(state->last != state->first){
		state->last = decr_last(state);
	}
}

// Funcao generica que movimenta o ultimo processo do estado A para o estado B
void move_processo(estado* leave, estado* enter){
	if(push_back_processo(enter, get_first_processo(leave)) == 0){
		rm_first_processo(leave);
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
	proc.pid = fork();
	if(proc.pid > 0){
		kill(proc.pid, SIGTSTP); // Pausa o processo filho
		push_back_processo(inicial, proc);
		// ACHO que nesse ponto o processo ja esta inicializado e ok, entao ja vou mandar pra fila de pronto
		move_processo(inicial, pronto);
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
	move_processo(execucao, finalizado); // Essa operacao nao pode falhar, senao da ruim
}

void faz_nada(){
	// literalmente
}

void executa(){
	kill(get_first_processo(execucao).pid, SIGCONT);
}

void encerra(){
	// por enquanto nao precisa fazer nada, o processo fica parado na lista de finalizados
	// vai sair no final quando o programa chamar a limpeza
}

int busy(){
	return (execucao->last == incr_first(execucao)) ? 1 : 0;
}

int main(int argc, char** argv){

	signal(SIGCHLD, handle_child);

	inicial = new_estado(30, faz_nada);
	pronto = new_estado(50, faz_nada); // Colocar aqui uma funcao que posiciona corretamente na fila
	// suspenso = new_estado(50); // Aguardando operacao de I/O
	execucao = new_estado(1, executa);
	finalizado = new_estado(30, encerra);

	// Puxar uma thread

	new_processo("teste", "10"); // Criar mais processos de teste

	// Colocar dentro de um loop, para ficar
	// sempre tentando colocar algum processo em execucao
	move_processo(pronto, execucao);

	int pid = get_back_processo(execucao).pid;
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


