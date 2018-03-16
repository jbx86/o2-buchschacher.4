// oss.c
#include "proj4.h"

int main() {
	const int maxTimeBetweenNewProcsNS = 0;
	const int maxTimeBetweenNewProcsSecs = NPS;

	const int rtProb = 100;	// Probability of generating a realtime process

	int pcbid;		// Shared memory ID of process control block table
	int clockid;		// Shared memory ID of simulated system clock
	int msgid;		// Message queue ID

	pcb *pcbTable;		// Pointer to table of process control blocks
	sim_time *ossClock;	// Pointer to simulated system clock
	message_buf buf;
	size_t buf_length;


	int quantum[4] = {10, 20, 40, 80};
	int inuse[18] = {0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0};

	pid_t pid;	// pid generated after a fork
	int queueNum;	// 0 for realitme, 1 for high, 2 for medium, 3 for low

	int i = 0;
	int newUser = 1;
	int ticks = 0;

// Setup IPC

	// Create memory segment for process control table
	if ((pcbid = shmget(PCBKEY, 18*sizeof(pcbTable), IPC_CREAT | 0666)) < 0 ) {
		perror("oss: pcbid");
		exit(1);
	}
	pcbTable = shmat(pcbid, NULL, 0);

	// Create memory segment for simulated system clock
	if ((clockid = shmget(CLKKEY, sizeof(sim_time), IPC_CREAT | 0666)) < 0 ) {
		perror("oss: clockid");
		exit(1);
	}
	ossClock = shmat(clockid, NULL, 0);

	// Initialze simulated clock to 0.0
	ossClock->sec = 0;
	ossClock->nano = 0;

	// Create message queue
	if ((msgid = msgget(MSGKEY, IPC_CREAT | 0666)) < 0) {
		perror("oss: msgget");
		exit(1);
	}

/*	// Initialize queue with ready message
	queueNum = 0;
	buf.mtype = queueNum + 1;
	sprintf(buf.mtext, "Message from OSS");
	buf_length = strlen(buf.mtext) + 1;
	if (msgsnd(msgid, &buf, buf_length, 0) < 0) {
		perror("oss: msgsend");
	}
*/

	// Initialize random generator
	srand(time(0));

// End Setup

	while (1) {

		// Wait for message
		if (msgrcv(msgid, &buf, MSGSZ, 0, 0) < 0) {
			perror("oss: msgrcv");
			exit(1);
		}

// Start critical section

		// Find next process in queue or advance clock
		if (buf.mtype != 5) {
			if (queueNum < 3) {
				queueNum++;
			}
			else {
				queueNum = 0;
				simadd(ossClock, 100);
				ticks++;
				printf("clock: %d.%09d\n", ossClock->sec, ossClock->nano);
			}
		}
		else {
			simadd(ossClock, quantum[queueNum]);
			ticks++;
			printf("clock: %d.%09d\n", ossClock->sec, ossClock->nano);
		}

		// Check if it's time spawn a user process
		if (newUser) {
			queueNum = 0; // Reset queue to zero when a new process is added
			int queueRoll;
			// Randomly assign processes as real-time or normal
			if ((queueRoll = rand() % 100) < rtProb) {
				pcbTable[0].queue = 0;
			}
			else {
				pcbTable[0].queue = 1;
			}

			pcbTable[0].arrival = *ossClock;

			// Fork and execute child process
			if ((pid = fork()) == 0) {
				execl("./user", "user", NULL);
				exit(0);
			}

			pcbTable[0].pid = pid;

			printBlock(0, pcbTable[0]);	

			while (i++ < 500000000); //give user time to initialize
			newUser = 0;

		}



		i++;

// End critical section

		// Send ready message
		buf.mtype = queueNum + 1;
		sprintf(buf.mtext, "Message from OSS");
		buf_length = strlen(buf.mtext) + 1;
		if (msgsnd(msgid, &buf, buf_length, 0) < 0) {
			perror("oss: msgsend");
		}


	}

// Cleanup IPC


	for (i = 0; i < 18; i++) {
		if (pcbTable[i].pid != 0) { 
			kill(pcbTable[i].pid, SIGINT);
		}
	}

	//wait(NULL);

	shmctl(pcbid, IPC_RMID, NULL); // Release process control block memeory
	shmctl(clockid, IPC_RMID, NULL); // Release simulated system clock memory
	msgctl(msgid, IPC_RMID, NULL); // Release message queue memory

	return 0;
}
