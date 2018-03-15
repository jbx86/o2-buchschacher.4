// user.c
#include "proj4.h"

int main() {
	int pcbShmid;		// Shared memory ID of process control block table
	int clkShmid;		// Shared memory ID of simclock
	int msgShmid;		// Message queue ID

	pcb *pcbTable;		// Pointer to table of process control blocks
	sim_time *ossClock;	// Pointer to simulated system clock
	message_buf buf;	// Message buffer
	size_t buf_length;	// Length of send message buffer

	pid_t pid = getpid();	// pid of this child
	int i;

// Setup IPC

	// Locate shared process control table
	if ((pcbShmid = shmget(PCBKEY, 18*sizeof(pcbTable), 0666)) < 0 ) {
		perror("user: shmget pcbShmid");
		exit(1);
	}
	pcbTable = shmat(pcbShmid, NULL, 0);

	// Locate shared simulated system clock
	if ((clkShmid = shmget(CLKKEY, sizeof(sim_time), 0666)) < 0 ) {
		perror("user: shmget clkShmid");
		exit(1);
	}
	ossClock = shmat(clkShmid, NULL, 0);

	// Locate message queue
	if ((msgShmid = msgget(MSGKEY, 0666)) < 0) {
		perror("user: shmget msgShmid");
		exit(1);
	}

	while (1) {

		// Block until message recieved
		if (msgrcv(msgShmid, &buf, MSGSZ, pcbTable->queue, 0) < 0) {
			perror("user: msgrcv");
			exit(1);
		}

// Start critical section

		printf("%ld: recived message %d at time %d.%09d\n", pcbTable[0].pid, buf.mtype, ossClock->sec, ossClock->nano);
		if ((pcbTable->queue > 0) && (pcbTable->queue < 4)) {
			pcbTable->queue++;
		}

// End critical section

		// send back mtype 5 to tell OSS a process was run at this level
		buf.mtype = 5;
		sprintf(buf.mtext, "Message from user");
		buf_length = strlen(buf.mtext) + 1;
		if (msgsnd(msgShmid, &buf, buf_length, 0) < 0) {
			perror("user: msgsend");
		}

	}

//printf("Message recieved\n");
/*
	// Rest of the program:
	i = 0;
	while ((pid != pcbTable[i].pid) && (i < 18)) {
		i++;
	}
	if (i < 18) {
		printBlock(i, pcbTable[i]);
	}
*/	
//


	return 0;
}
