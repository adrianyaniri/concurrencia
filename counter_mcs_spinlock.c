#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>


#define NUM_THREADS	     4
#define MAX_COUNT 10000000

// Just used to send the index of the id
struct tdata {
	int tid;
};

// Struct for MCS spinlock
struct mcs_spinlock {
	struct mcs_spinlock *next;
	unsigned locked;
};

struct mcs_spinlock *mcs_tail = NULL;

int counter = 0;

void lock(struct mcs_spinlock *node) {
	node->next = NULL;
	struct mcs_spinlock *predecessor = node;
	predecessor = __sync_lock_test_and_set(&mcs_tail, predecessor); // it's equivalent to fetchAndStore
	if (predecessor != NULL) {
		node->locked = 1;
		predecessor->next = node;
		while (node->locked);
	}
}

void unlock(struct mcs_spinlock *node) {
	if (! node->next) {
		if (__sync_bool_compare_and_swap(&mcs_tail, node, NULL) ) {
			return;
		} else {
			// Another process executed fetchAndStore but
			// didn't asssign next yet, so wait
			while (! node->next);
		}
	}

	node->next->locked = 0;
}
	
void *count(void *ptr) {
	long i, max = MAX_COUNT/NUM_THREADS;
	int tid = ((struct tdata *) ptr)->tid;

	struct mcs_spinlock node;

	for (i=0; i < max; i++) {
		lock(&node);
		counter += 1; // The global variable, i.e. the critical section
		unlock(&node);
	}

	printf("End %d counter: %d\n", tid, counter);
	pthread_exit(NULL);
}

int main (int argc, char *argv[]) {
	pthread_t threads[NUM_THREADS];
	int rc, i;
	struct tdata id[NUM_THREADS];

	for(i=0; i<NUM_THREADS; i++){
		id[i].tid = i;
		rc = pthread_create(&threads[i], NULL, count, (void *) &id[i]);
		if (rc){
			printf("ERROR; return code from pthread_create() is %d\n", rc);
			exit(-1);
		}
	}

	for(i=0; i<NUM_THREADS; i++){
		pthread_join(threads[i], NULL);
	}

	printf("Counter value: %d Expected: %d\n", counter, MAX_COUNT);
	return 0;
}

