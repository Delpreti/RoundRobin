#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char** argv){

	if(argc < 1){
		return 0;
	}

	int i = 0;
	int x = atoi(argv[1]);
	for(i = 0; i < x; i++){
		printf("%d\n", i + 1);
		sleep(1);
	}

	return 0;
}