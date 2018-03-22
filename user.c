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

	int i;

// Setup IPC

	// Locate shared process control table
	if ((pcbShmid = shmget(PCBKEY, SIZE*sizeof(pcbTable), 0666)) < 0 ) {
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

	// Find this processes simulated PID
	int myPid = -1;
	for (i = 0; i < SIZE; i++) {
		if (pcbTable[i].pid == getpid())
			break;
	}
	if (i != SIZE)
		myPid = i;		

	srand(pcbTable[myPid].seed);
	

	printf("USER: PID %d is running!\n", myPid);

// Main loop

	while (pcbTable[myPid].termFlag == 0) {

		// Wait on message from OSS
		printf("%d: Waiting for message\n", myPid);
		if (msgrcv(msgShmid, &buf, MSGSZ, myPid + 1, 0) < 0) {
			perror("user: msgrcv");
			exit(1);
		}

		if ((rand() % 100) < 0) {
			printf("%d: Will terminate\n", myPid);
			pcbTable[myPid].termFlag = 1;
		}
		else {
			printf("%d: Will continue to run\n", myPid);
			pcbTable[myPid].termFlag = 0;
		}

		printf("%d: In critical section\n", myPid);
		sleep(1);

		printf("%d: Send message to OSS\n", myPid);
		
		// send back mtype 5 to tell OSS a process was run at this level
		buf.mtype = SIZE + 1;
		sprintf(buf.mtext, "Message from user");
		buf_length = strlen(buf.mtext) + 1;
		if (msgsnd(msgShmid, &buf, buf_length, 0) < 0) {
			perror("user: msgsend");
		}
	}

	printf("%d: Terminating\n", myPid);

	exit(0);
}
