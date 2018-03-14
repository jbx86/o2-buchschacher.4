// user.c
#include "proj4.h"

int main() {
	int pcbShmid;		// Shared memory ID of process control block table
	int clkShmid;		// Shared memory ID of simclock
	int msgShmid;		// Message queue ID

	pcb *pcbTable;		// Pointer to table of process control blocks
	sim_time *ossClock;	// Pointer to simulated system clock

	pid_t pid = getpid();	// pid of this child
	int i;

	// Create memory segment for process control table
	if ((pcbShmid = shmget(PCBKEY, 18*sizeof(pcbTable), 0666)) < 0 ) {
		perror("user: pcbShmid");
		exit(1);
	}
	pcbTable = shmat(pcbShmid, NULL, 0);

	// Create memory segment for simulated system clock
	if ((clkShmid = shmget(CLKKEY, sizeof(sim_time), 0666)) < 0 ) {
		perror("user: clkShmid");
		exit(1);
	}
	ossClock = shmat(clkShmid, NULL, 0);




	// Rest of the program:
	i = 0;
	while ((pid != pcbTable[i].pid) && (i < 18)) {
		i++;
	}
	if (i < 18) {
		printBlock(i, pcbTable[i]);
	}
	

	return 0;
}
