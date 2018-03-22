// oss.c
#include "proj4.h"

int dequeue(int[*]);
void enqueue(int[*], int);
sim_time addTime(sim_time, int, int);

int main() {
	const int maxTimeBetweenNewProcsSecs = 1;
	const int maxTimeBetweenNewProcsNS = NPS;	

	int pcbid;		// Shared memory ID of process control block table
	int clockid;		// Shared memory ID of simulated system clock
	int msgid;		// Message queue ID

	pcb *pcbTable;		// Pointer to table of process control blocks
	sim_time *ossClock;	// Pointer to simulated system clock

	message_buf buf;
	size_t buf_length;

	sim_time nextFork;	// Time at which the next process will be generated
	
	int quantum[5] = {10, 20, 40, 80, 100};
	int inUse[SIZE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	int queue[5][SIZE] = {{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},		// Realtime queue
				{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	// High priority queue
				{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	// Mid priority queue
				{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	// Low priority queue
				{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}};	// Blocked queue

	pid_t pid;	// pid generated after a fork	

	int i = 0;
	//int newUser = 1;
	int ticks = 0;

	// Open log file for output
	FILE *fp;
	fp = fopen("data.log", "w");
	if (fp == NULL) {
		fprintf(stderr, "Can't open file\n");
		exit(1);
	}

	fprintf(fp, "Activity Log:\n");

// Setup IPC

	// Create memory segment for process control table
	if ((pcbid = shmget(PCBKEY, SIZE*sizeof(pcbTable), IPC_CREAT | 0666)) < 0 ) {
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


	srand(time(0));				// Initialize rand()
	nextFork = addTime(*ossClock, 1, 0);	// Setup when to generate a process

	int spawnTest = 2;
	int ossloop = 10;
	//alarm(3);

	while(ossloop--) {



	// Generate new process
		if (spawnTest--) {

			// Find next open process control block
			for (i = 0; i < SIZE; i++)
				if (inUse[i] == 0)
					break;
	
			// If an open process control block is found
			if (i < SIZE) {
				// Initialize control block
				inUse[i] = 1;
				enqueue(queue[0], i);
				pcbTable[i].arrival = *ossClock;
				fprintf(fp, "OSS: Generating process with PID %d and putting it in queue %d at time %d:%09d\n",
						i, 0, pcbTable[i].arrival.sec, pcbTable[i].arrival.nano) ;
	
				// fork/execl child
				if ((pid = fork()) == 0) {
					execl("./user", "user", NULL);
					fprintf(stderr, "Fail to execute child process\n");
				}
				else {
					pcbTable[i].pid = pid;
					pcbTable[i].seed = rand();
				}
			}
		}

// Scheduler

		// Find next process to run
		int lvl;
		int nextPid;
		for (lvl = 0; lvl < 4; lvl++)
			if ((nextPid = dequeue(queue[lvl])) != -1)
				break;
	
		// Dispatch next ready process or increment to generation
		if (nextPid != -1) {
			fprintf(fp, "OSS: Dispatching process with PID %d from queue %d at time %d:%09d\n",
				nextPid, lvl, ossClock->sec, ossClock->nano);


			// Give nextPid a timeslice
			pcbTable[nextPid].queue = lvl;
			pcbTable[nextPid].timeslice = quantum[lvl];
			printf("OSS: Giving PID %d timeslice of %dns\n", nextPid, pcbTable[nextPid].timeslice);

			// Send message to nextPid
			printf("OSS: Sending message to %d\n", nextPid);
			pcbTable[nextPid].lastburst = pcbTable[nextPid].timeslice;
			buf.mtype = nextPid + 1;
			sprintf(buf.mtext, "Message from OSS");
			buf_length = strlen(buf.mtext) + 1;
			if (msgsnd(msgid, &buf, buf_length, 0) < 0) {
				perror("oss: msgsend");
			}


			// Block wait for nextPid to be finished 
			if (msgrcv(msgid, &buf, MSGSZ, SIZE + 1, 0) < 0) {
				perror("oss: msgrcv");
				exit(1);
			}
			printf("OSS: PID %d finished, entering critical section\n", nextPid);
			sleep(1);


			*ossClock = addTime(*ossClock, 0, pcbTable[nextPid].lastburst);		// Advance simclock by time used
			fprintf(fp, "OSS: total time this dispatch was %d nanoseconds\n", pcbTable[nextPid].lastburst);

			if ((pcbTable[nextPid].queue > 0) && (pcbTable[nextPid].queue < 3))	// Normal class programs down in priority
				pcbTable[nextPid].queue++;

			// Enqueue nextPid if it has not terminated
			if (pcbTable[nextPid].termFlag == 0) {
				enqueue(queue[pcbTable[nextPid].queue], nextPid);
				fprintf(fp, "OSS: Putting process with PID %d into queue %d\n", nextPid, pcbTable[nextPid].queue);
			}
			fprintf(stderr, "OSS: PID %d ran for %d, completing at %d:%09d\n",
                                nextPid, pcbTable[nextPid].lastburst, ossClock->sec, ossClock->nano);
		}
		else {
			*ossClock = addTime(*ossClock, 0, 100);
			//printf("OSS: no processes in queue, advancing clock to %d.%09d\n", ossClock->sec, ossClock->nano);
		}
	}

// Cleanup IPC

	for (i = 0; i < SIZE; i++)
		if (inUse[i] == 1) {
			//kill(pcbTable[i].pid, SIGINT);
			//wait(pcbTable[i].pid);
		}


	shmctl(pcbid, IPC_RMID, NULL); // Release process control block memeory
	shmctl(clockid, IPC_RMID, NULL); // Release simulated system clock memory
	msgctl(msgid, IPC_RMID, NULL); // Release message queue memory

	fclose(fp);

	return 0;
}

//----- Other Functions --------------------------------------------------------

sim_time addTime(sim_time inputTime, int sec, int nano) {
	inputTime.nano += nano;
	inputTime.sec += sec + (inputTime.nano / NPS);
	inputTime.nano = inputTime.nano % NPS;
	return inputTime;
}

int dequeue(int queue[]) {
	int nextPid = queue[0];	// Copy value from first element in queue
	int i;

	// Shift each value down one
	for (i = 0; i < (SIZE - 1); i++) {
		queue[i] = queue[i + 1];
	}
	queue[SIZE - 1] = -1;

	return nextPid;	// Return value that was in first element of queue
}

void enqueue(int queue[], int simPid) {
	int i;

	// Traverse queue until open element is found
	for (i = 0; i < SIZE; i++) {
		if (queue[i] == -1) {
			// Insert simulate pid at end of queue and return to main
			queue[i] = simPid;
			return;
		}
	}
	
	// Overflow error if no open elements in queue
	fprintf(stderr, "queue overflow\n");
	exit(1);
}
