// oss.c
#include "proj4.h"

int main() {
	const int maxTimeBetweenNewProcsNS = 0;
	const int maxTimeBetweenNewProcsSecs = NPS;

	int pcbShmid;		// Shared memory ID of process control block table
	int clkShmid;		// Shared memory ID of simclock
	int msgShmid;		// Message queue ID

	pcb *pcbTable;		// Pointer to table of process control blocks
	sim_time *ossClock;	// Pointer to simulated system clock

	pid_t pid;	// pid generated after a fork
	int i = 0;


	// Create memory segment for process control table
	if ((pcbShmid = shmget(PCBKEY, 18*sizeof(pcbTable), IPC_CREAT | 0666)) < 0 ) {
		perror("oss: pcbShmid");
		exit(1);
	}
	pcbTable = shmat(pcbShmid, NULL, 0);

	// Create memory segment for simulated system clock
	if ((clkShmid = shmget(CLKKEY, sizeof(sim_time), IPC_CREAT | 0666)) < 0 ) {
		perror("oss: clkShmid");
		exit(1);
	}
	ossClock = shmat(clkShmid, NULL, 0);

	// Initialze simulated clock to 0.0
	ossClock->sec = 0;
	ossClock->nano = 0;

	// Create a process
	if ((pid = fork()) == 0) {
		execl("./user", "user", NULL);
		exit(0);
	}

	i = ((long)pid) % 18;
	pcbTable[i].pid = pid;
	pcbTable[i].queue = i;


	sleep(1);

	// Cleanup IPC
	shmctl(pcbShmid, IPC_RMID, NULL);
	shmctl(clkShmid, IPC_RMID, NULL);

	return 0;
}
