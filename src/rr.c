#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdbool.h>
#include <threads.h>
#include <time.h>

#include "heap.h"
#include "queue.h"
#include "rr.h"

extern processo nulo;

// Um estado contem uma (geralmente) ou mais filas
typedef struct {
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
estado* new_estado(int capacidade, void (*fun)(), int quant_filas){
	estado* state = malloc(sizeof(estado));
	state->f_high = 0;
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
// estado* suspenso; // Aguardando operacao de I/O
estado* execucao;
estado* finalizado;

fila* get_enter_fila(estado *state, processo proc){
	return state == pronto ? state->f_list[proc.priority] : state->f_list[state->f_high];
}

fila* get_leave_fila(estado *state){
	return state->f_list[state->f_high]; // retorna sempre a fila de alta prioridade
}

// Funcao generica que movimenta um processo
// Retorna 0 em caso de sucesso, 1 em caso de falha
int move_processo_2(fila* leave, estado* enter){
	processo proc = get_first_processo(leave);
	if(push_back_processo(get_enter_fila(enter, proc), proc) == 0){
		rm_first_processo(leave);
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

// Funcao para criacao de um novo processo
// Nao precisa retornar o processo criado, pois todo processo vai direto para o estado inicial
void new_processo(char* nome, char* parametros){
	processo proc;
	proc.priority = pronto->f_high; // Processo possui alta prioridade quando criado
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
	// Puxar uma thread (?)

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


