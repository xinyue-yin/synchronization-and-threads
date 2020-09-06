#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include "uthread.h"
#include "uthread_mutex_cond.h"

#ifdef VERBOSE
#define VERBOSE_PRINT(S, ...) printf (S, ##__VA_ARGS__);
#else
#define VERBOSE_PRINT(S, ...) ;
#endif

#define MAX_OCCUPANCY      3
#define NUM_ITERATIONS     100
#define NUM_PEOPLE         20
#define FAIR_WAITING_COUNT 4

/**
 * You might find these declarations useful.
 */
enum Endianness {LITTLE = 0, BIG = 1};
const static enum Endianness oppositeEnd [] = {BIG, LITTLE};

struct Well {
  // TODO
  uthread_mutex_t mutex;
  int endianness;
  int numP;
  int sameEnd;
  int wait;
  int waitLine[2];
};

struct Well* createWell() {
  struct Well* Well = malloc (sizeof (struct Well));
  // TODO
  Well->mutex = uthread_mutex_create();
  Well->endianness = -1;
  Well->numP = 0;
  Well->sameEnd = 0;
  Well->wait = 0;
  Well->waitLine[0] = 0;
  Well->waitLine[1] = 0;
  return Well;
}

struct Well* Well;

#define WAITING_HISTOGRAM_SIZE (NUM_ITERATIONS * NUM_PEOPLE)
int             entryTicker;                                          // incremented with each entry
int             waitingHistogram         [WAITING_HISTOGRAM_SIZE];
int             waitingHistogramOverflow;
uthread_mutex_t waitingHistogrammutex;
int             occupancyHistogram       [2] [MAX_OCCUPANCY + 1];
uthread_cond_t letIn[2];

void enterWell (enum Endianness g) {
  // TODO
  uthread_mutex_lock(Well->mutex);
  if (Well->endianness == -1) {
    Well->endianness = g;
  }
  Well->waitLine[g]++;
  entryTicker++;
  int waitingTimeStart = entryTicker;
  while (Well->endianness!=g || Well->numP >= MAX_OCCUPANCY || Well->wait) {
    if (Well->sameEnd >= 5){
      Well->wait = 1;
    }
    uthread_cond_wait(letIn[g]);
  }
  assert(Well->numP <= MAX_OCCUPANCY);
  assert(Well->endianness == g);
  recordWaitingTime(entryTicker-waitingTimeStart);
  Well->numP++;
  Well->sameEnd++;
  Well->waitLine[g]--;
  occupancyHistogram[g][Well->numP]++;
  uthread_mutex_unlock(Well->mutex);
}


void leaveWell() {
  // TODO
  uthread_mutex_lock(Well->mutex);
  Well->numP--;
  if (Well->wait == 1 && Well->waitLine[oppositeEnd[Well->endianness]] != 0){
    if (Well->numP == 0){
      Well->wait = 0;
      Well->endianness = oppositeEnd[Well->endianness];
      Well->sameEnd = 0;
      uthread_cond_broadcast(letIn[Well->endianness]);
    }
  } else if (Well->numP == 0 && Well->waitLine[oppositeEnd[Well->endianness]] != 0){
    Well->endianness = oppositeEnd[Well->endianness];
    uthread_cond_broadcast(letIn[Well->endianness]);
  } else {
    Well->wait = 0;
    uthread_cond_signal(letIn[Well->endianness]);
  }
  uthread_mutex_unlock(Well->mutex);
}

void recordWaitingTime (int waitingTime) {
  uthread_mutex_lock (waitingHistogrammutex);
  if (waitingTime < WAITING_HISTOGRAM_SIZE)
    waitingHistogram [waitingTime] ++;
  else
    waitingHistogramOverflow ++;
  uthread_mutex_unlock (waitingHistogrammutex);
}

//
// TODO
// You will probably need to create some additional produres etc.
//
void* lilliputian(void* endianness){
  int end = (int) endianness;
  for (int i = 0; i< NUM_ITERATIONS; i++) {
    enterWell(end);
    for(int i = 0; i< NUM_PEOPLE; i++){
      uthread_yield();
    }
    leaveWell();
    for(int i = 0; i< NUM_PEOPLE; i++){
      uthread_yield();
    }
  }
  return;
}

int main (int argc, char** argv) {
  uthread_init (1);
  Well = createWell();
  uthread_t pt [NUM_PEOPLE];
  waitingHistogrammutex = uthread_mutex_create ();

  // TODO
  entryTicker = 0;
  letIn[0] = uthread_cond_create(Well->mutex);
  letIn[1] = uthread_cond_create(Well->mutex);

  for(int i = 0; i< NUM_PEOPLE; i++){
    int e = random() % 2;
    pt[i] = uthread_create(lilliputian, (void*) e);
  }
  for(int i = 0; i< NUM_PEOPLE; i++){
    uthread_join(pt[i], 0);
  }

  printf ("Times with 1 little endian %d\n", occupancyHistogram [LITTLE]   [1]);
  printf ("Times with 2 little endian %d\n", occupancyHistogram [LITTLE]   [2]);
  printf ("Times with 3 little endian %d\n", occupancyHistogram [LITTLE]   [3]);
  printf ("Times with 1 big endian    %d\n", occupancyHistogram [BIG] [1]);
  printf ("Times with 2 big endian    %d\n", occupancyHistogram [BIG] [2]);
  printf ("Times with 3 big endian    %d\n", occupancyHistogram [BIG] [3]);
  printf ("Waiting Histogram\n");
  for (int i=0; i<WAITING_HISTOGRAM_SIZE; i++)
    if (waitingHistogram [i])
      printf ("  Number of times people waited for %d %s to enter: %d\n", i, i==1?"person":"people", waitingHistogram [i]);
  if (waitingHistogramOverflow)
    printf ("  Number of times people waited more than %d entries: %d\n", WAITING_HISTOGRAM_SIZE, waitingHistogramOverflow);
}
