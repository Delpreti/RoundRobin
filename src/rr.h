#ifndef __ROUND_ROBIN_IMP_H
#define __ROUND_ROBIN_IMP_H

#ifndef bool
 #define bool char
#endif

// Todo processo pode ser identificado por seu PID.
// Deixar como struct por enquanto, pode ser que o simulador precise guardar mais informacoes
typedef struct {
	pid_t pid;
	int priority; // Quanto menor, mais prioridade
} processo;

#endif
