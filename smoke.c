#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include "uthread.h"
#include "uthread_mutex_cond.h"

#define NUM_ITERATIONS 10000

#ifdef VERBOSE
#define VERBOSE_PRINT(S, ...) printf (S, ##__VA_ARGS__);
#else
#define VERBOSE_PRINT(S, ...) ;
#endif

struct Agent {
  uthread_mutex_t mutex;
  uthread_cond_t  match;
  uthread_cond_t  paper;
  uthread_cond_t  tobacco;
  uthread_cond_t  smoke;
};

struct Agent* createAgent() {
  struct Agent* agent = malloc (sizeof (struct Agent));
  agent->mutex   = uthread_mutex_create();
  agent->paper   = uthread_cond_create (agent->mutex);
  agent->match   = uthread_cond_create (agent->mutex);
  agent->tobacco = uthread_cond_create (agent->mutex);
  agent->smoke   = uthread_cond_create (agent->mutex);
  return agent;
}

//
// TODO
// You will probably need to add some procedures and struct etc.
//

int available = 0;
enum Resource            {    MATCH = 1, PAPER = 2,   TOBACCO = 4};
char* resource_name [] = {"", "match",   "paper", "", "tobacco"};

int signal_count [5];  // # of times resource signalled
int smoke_count  [5];  // # of times smoker with resource smoked


struct Smoker {
  uthread_mutex_t mutex;
  uthread_cond_t smoke;
  uthread_cond_t letSmoke;
  int type;
};

struct Smoker* createSmoker(struct Agent* a, int type){
  struct Smoker* smoker = malloc (sizeof (struct Smoker));
  smoker->mutex = a->mutex;
  smoker->smoke = a->smoke;
  smoker->letSmoke = uthread_cond_create(smoker->mutex);
  smoker->type = type;
}

void* smoker(void* smoker) {
  struct Smoker* s = smoker;
  uthread_mutex_lock(s->mutex);
    while(1) {
      uthread_cond_wait(s->letSmoke);
      if (available == 7 - s->type){
        smoke_count [s->type] ++;
        available = 0;
        uthread_cond_signal(s->smoke);
      }
    }
  uthread_mutex_unlock(s->mutex);
}

struct Signaler {
  uthread_mutex_t mutex;
  uthread_cond_t sigReceive;
  uthread_cond_t letSmoke1;
  uthread_cond_t letSmoke2;
  int type;
};

struct Signaler* createSignaler(struct Smoker* s, struct Smoker* s1, struct Smoker* s2, struct Agent* a){
  struct Signaler* signaler = malloc(sizeof(struct Signaler));
  signaler->mutex = s->mutex;
  signaler->type = s->type;
  signaler->letSmoke1 = s1->letSmoke;
  signaler->letSmoke2 = s2->letSmoke;
  switch(s->type) {
    case MATCH:
      signaler->sigReceive = a->match;
      break;
    case PAPER:
      signaler->sigReceive = a->paper;
      break;
    case TOBACCO:
      signaler->sigReceive = a->tobacco;
      break;
    default:
      break;
  }
}

void* signaler(void* signaler) {
  struct Signaler* sig = signaler;
  uthread_mutex_lock(sig->mutex);
    while(1){
      uthread_cond_wait(sig->sigReceive);
      available += sig->type;
      uthread_cond_signal(sig->letSmoke1);
      uthread_cond_signal(sig->letSmoke2);
    }
  uthread_mutex_unlock(sig->mutex);
}
/**
 * You might find these declarations helpful.
 *   Note that Resource enum had values 1, 2 and 4 so you can combine resources;
 *   e.g., having a MATCH and PAPER is the value MATCH | PAPER == 1 | 2 == 3
 */

/**
 * This is the agent procedure.  It is complete and you shouldn't change it in
 * any material way.  You can re-write it if you like, but be sure that all it does
 * is choose 2 random reasources, signal their condition variables, and then wait
 * wait for a smoker to smoke.
 */
void* agent (void* av) {
  struct Agent* a = av;
  static const int choices[]         = {MATCH|PAPER, MATCH|TOBACCO, PAPER|TOBACCO};
  static const int matching_smoker[] = {TOBACCO,     PAPER,         MATCH};

  uthread_mutex_lock (a->mutex);
    for (int i = 0; i < NUM_ITERATIONS; i++) {
      int r = random() % 3;
      signal_count [matching_smoker [r]] ++;
      int c = choices [r];
      if (c & MATCH) {
        VERBOSE_PRINT ("match available\n");
        uthread_cond_signal (a->match);
      }
      if (c & PAPER) {
        VERBOSE_PRINT ("paper available\n");
        uthread_cond_signal (a->paper);
      }
      if (c & TOBACCO) {
        VERBOSE_PRINT ("tobacco available\n");
        uthread_cond_signal (a->tobacco);
      }
      VERBOSE_PRINT ("agent is waiting for smoker to smoke\n");
      uthread_cond_wait (a->smoke);
    }
  uthread_mutex_unlock (a->mutex);
  return NULL;
}

int main (int argc, char** argv) {
  uthread_init (7);
  struct Agent*  a = createAgent();
  // TODO
  struct Smoker* sm = createSmoker(a, MATCH);
  struct Smoker* sp = createSmoker(a, PAPER);
  struct Smoker* st = createSmoker(a, TOBACCO);
  struct Signaler* ssm = createSignaler(sm, sp, st, a);
  struct Signaler* ssp = createSignaler(sp, sm, st, a);
  struct Signaler* sst = createSignaler(st, sm, sp, a);
  uthread_create (smoker, sm);
  uthread_create (smoker, sp);
  uthread_create (smoker, st);
  uthread_create (signaler, ssm);
  uthread_create (signaler, ssp);
  uthread_create (signaler, sst);

  uthread_join (uthread_create (agent, a), 0);
  assert (signal_count [MATCH]   == smoke_count [MATCH]);
  assert (signal_count [PAPER]   == smoke_count [PAPER]);
  assert (signal_count [TOBACCO] == smoke_count [TOBACCO]);
  assert (smoke_count [MATCH] + smoke_count [PAPER] + smoke_count [TOBACCO] == NUM_ITERATIONS);
  printf ("Smoke counts: %d matches, %d paper, %d tobacco\n",
          smoke_count [MATCH], smoke_count [PAPER], smoke_count [TOBACCO]);
}
