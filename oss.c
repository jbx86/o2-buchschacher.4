// oss.c
#include "proj4.h"

int dequeue(int[*]);
void enqueue(int[*], int);
sim_time addTime(sim_time, int, int);

int pcbid;	// Shared memory ID of process control block table
int clockid;	// Shared memory ID of simulated system clock
int msgid;	// Message queue ID
FILE *fp;	// Log file

void sig_handler(int signo) {
	if (signo == SIGINT) {
		shmctl(pcbid, IPC_RMID, NULL);		// Release process control block memeory
		shmctl(clockid, IPC_RMID, NULL);	// Release simulated system clock memory
		msgctl(msgid, IPC_RMID, NULL);		// Release message queue memory
		fclose(fp);
	}
}

int main() {
	const int maxTimeBetweenNewProcsSecs = 1;
	const int maxTimeBetweenNewProcsNS = NPS - 1;
	const int dispatchMin = 100;
	const int dispatchMax = 10000;
	const int usersMax = 100;
	const sim_time zeroClock = {0, 0};

	pcb *pcbTable;		// Pointer to table of process control blocks
	sim_time *ossClock;	// Pointer to simulated system clock

	message_buf buf;
	size_t buf_length;

	sim_time nextFork;	// Time at which the next process will be generated
	
	int quantum[5] = {10, 20, 40, 80, 100};
	int inUse[SIZE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	int queue[5][SIZE] =   {{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	// Realtime queue
				{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	// High priority queue
				{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	// Mid priority queue
				{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	// Low priority queue
				{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}};	// Blocked queue

	pid_t pid;	// pid generated after a fork
	int i;

	// Open log file for output
	fp = fopen("data.log", "w");
	if (fp == NULL) {
		fprintf(stderr, "Can't open file\n");
		exit(1);
	}

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
	*ossClock = zeroClock;

	// Create message queue
	if ((msgid = msgget(MSGKEY, IPC_CREAT | 0666)) < 0) {
		perror("oss: msgget");
		exit(1);
	}


	srand(time(0));				// Initialize rand()
	nextFork = addTime(*ossClock, 1, 0);	// Setup when to generate a process

//	int spawnTest = 3;
	int ossloop = 100;


	int usersForked = 0;
	int usersComplete = 0;

	//alarm(3);

// Main loop

	while(ossloop--) {
	printf("%d\n", ossloop);

	// Generate new process
		if ((isPast(*ossClock, nextFork) == 1) && (usersForked < usersMax)) {
			
			// Find next open process control block i
			for (i = 0; i < SIZE; i++)
				if (inUse[i] == 0)
					break;
	
			// If an open process control block is found
			if (i < SIZE) {

				usersForked++;

				// Initialize control block
				inUse[i] = 1;
				pcbTable[i].arrival = *ossClock;
				pcbTable[i].cpuTimeUsed = zeroClock;
				pcbTable[i].runtime = zeroClock;
				pcbTable[i].lastburst = 0;
				pcbTable[i].timeslice = 0;
				pcbTable[i].termFlag = 0;
				pcbTable[i].suspFlag = 0;
				pcbTable[i].seed = rand();
				pcbTable[i].simpid = usersForked;

				// Assign 30% of processes generated to real-time class
				if (pctToBit(rand(), 30))
					pcbTable[i].queue = 0;
				else
					pcbTable[i].queue = 1; 
				enqueue(queue[pcbTable[i].queue], i);

				fprintf(fp, "OSS: Generating process with PID %d and putting it in queue %d at time %d:%09d\n",
						pcbTable[i].simpid, pcbTable[i].queue, pcbTable[i].arrival.sec, pcbTable[i].arrival.nano) ;
	
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
			nextFork = addTime(*ossClock, 1, 0);
		}

	// Scheduler

		int lvl;	// Queue level
		int nextPid;	// Local pid

		// Find next process to run
		for (lvl = 0; lvl < 4; lvl++)
			if ((nextPid = dequeue(queue[lvl])) != -1)
				break;
	
		// Dispatch next ready process or increment to generation
		if (nextPid != -1) {

			// Increment clock for amount of work done by this dispatch
			int dispatchNano = (rand() % (dispatchMax - dispatchMin + 1)) + dispatchMin;
			*ossClock = addTime(*ossClock, 0, dispatchNano);

			fprintf(fp, "OSS: Dispatching process with PID %d from queue %d at time %d:%09d,\nOSS: total time spent in this dispatch was %d nanoseconds\n",
					pcbTable[nextPid].simpid, lvl, ossClock->sec, ossClock->nano, dispatchNano);


			// Give nextPid a timeslice
			pcbTable[nextPid].timeslice = quantum[pcbTable[nextPid].queue];
			printf("OSS: Giving PID %d timeslice of %d nanoseconds\n", pcbTable[nextPid].simpid, pcbTable[nextPid].timeslice);

			// Send message to nextPid
			printf("OSS: Sending message to %d\n", pcbTable[nextPid].simpid);
			buf.mtype = nextPid + 1;
			sprintf(buf.mtext, "Message from OSS");
			buf_length = strlen(buf.mtext) + 1;
			if (msgsnd(msgid, &buf, buf_length, 0) < 0) {
				perror("oss: msgsend");
				exit(1);
			}

			// Block wait for nextPid to be finished 
			if (msgrcv(msgid, &buf, MSGSZ, SIZE + 1, 0) < 0) {
				perror("oss: msgrcv");
				exit(1);
			}
			
			*ossClock = addTime(*ossClock, 0, pcbTable[nextPid].lastburst);
			pcbTable[nextPid].cpuTimeUsed = addTime(*ossClock, 0, pcbTable[nextPid].lastburst);

			// Enqueue nextPid if it has not terminated
			if (pcbTable[nextPid].termFlag == 0) {

				// Move high and middle priority processes down one level and requeue
				if ((pcbTable[nextPid].queue > 0) && (pcbTable[nextPid].queue < 3))
					pcbTable[nextPid].queue++;
				enqueue(queue[pcbTable[nextPid].queue], nextPid);
				fprintf(fp, "OSS: Receiving that process with PID %d ran for %d nanoseconds\n", pcbTable[nextPid].simpid, pcbTable[nextPid].lastburst);
				fprintf(fp, "OSS: Putting process with PID %d into queue %d\n", pcbTable[nextPid].simpid, pcbTable[nextPid].queue);

			}
			else {

				// Mark block as open
				inUse[nextPid] = 0;
				usersComplete++;
				fprintf(fp, "OSS: Receiving that process with PID %d ran for %d nanoseconds,\n", pcbTable[nextPid].simpid, pcbTable[nextPid].lastburst);
				fprintf(fp, "OSS: not using all of it's time quantum\n");

			}

			//fprintf(stderr, "OSS: PID %d ran for %d, completing at %d:%09d\n", nextPid, pcbTable[nextPid].lastburst, ossClock->sec, ossClock->nano);
		}
		else {
			fprintf(fp, "OSS: No processes in queue\n");
			*ossClock = nextFork;
			//printf("OSS: no processes in queue, advancing clock to %d:%09d\n", ossClock->sec, ossClock->nano);
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

int isPast(sim_time clock, sim_time epoch) {
	// Convert sim_time values to long ints
	long longClock = ((long)clock.sec * NPS) + clock.nano;
	long longEpoch = ((long)epoch.sec * NPS) + epoch.nano;

	//printf("%ld\n%ld", longClock, longEpoch);

	// Return 1 if clock is after epoch
	if (longClock >= longEpoch)
		return 1;
	else
		return 0;

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
