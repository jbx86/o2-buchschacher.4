// OSS.c
#include "proj4.h"

int main() {
	const int maxTimeBetweenNewProcsNS = 0;
	const int maxTimeBetweenNewProcsSecs = NPS;
	int shmid;
	pcb *pcbTable;
	pid_t pid;
	int i;


	if ((shmid = shmget(KEY, 18*sizeof(pcbTable), IPC_CREAT | 0666)) < 0 ) {
		perror("oss: shmid");
		exit(1);
	}

	pcbTable = shmat(shmid, NULL, 0);
	shmctl(shmid, IPC_RMID, NULL);

	//pcbTable[1].start.sec = 2;
	//pcbTable[2] = pcbTable[1];
	//pcbTable[1].start.sec++;
	//printf("Block 1 start sec: %d\n", pcbTable[1].start.sec);
	//printf("Block 2 start sec: %d\n", pcbTable[2].start.sec);

	for (i = 0; i < 18; i++) {
		if ((pid = fork()) == 0) {
			exit(0);
		}
		pcbTable[i].pid = pid;
		pcbTable[i].queue = i;
		printBlock(i, pcbTable[i]);
	}

	shmdt(pcbTable);	
	//shmctl(shmid, IPC_RMID, NULL);

	return 0;
}
