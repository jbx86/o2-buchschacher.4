#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>

#define PCBKEY 1077
#define CLKKEY 1138
#define MSGKEY 4242
#define MSGSZ 256
#define NPS 1000000000

typedef struct {
	int sec;
	int nano;
} sim_time;

typedef struct {
	pid_t pid;
	sim_time start;
	sim_time cpuTimeUsed;
	sim_time runtime;
	int lastBurst;
	int queue;
} pcb;

typedef struct msgbuf {
	long mtype;
	char mtext[MSGSZ];
} message_buf;

// Add a number of nanoseconds to a sim_time
void simadd(sim_time *time, int increment) {
	if (increment < 0) {
		fprintf(stderr, "simadd error: function cannot be called with a negative increment\n");
		exit(1);
	}
	
	time->nano += increment;
	
	if (time->nano >= NPS) {
		time->sec += (time->nano / NPS);
		time->nano = (time->sec % NPS);
	}
}

// Calculate number of time that has passed between two times
int simdiff(sim_time simclock, sim_time epoch) {
	if (simclock.sec < epoch.sec) {
		fprintf(stderr, "simdiff error: first time must be greater than second time");
		exit(1);
	}
	else if((simclock.sec == epoch.sec) && (simclock.nano < epoch.nano)) {
		fprintf(stderr, "simdiff error: first time must be greater than second time");
		exit(1);
	}

	while (simclock.sec > epoch.sec) {
		simclock.sec--;
		simclock.nano += NPS;
	}
	
	return simclock.nano - epoch.nano;
}

void printBlock(int i, pcb a) {
	printf("Block %d:\n\tPID: %ld\n\tPriority: %d\n", i, a.pid, a.queue);
}
