/*
 *	File	: covidTrace.c
 *
 *	Title	: Demo Producer/Consumer.
 *
 *	Short	: A solution to the producer consumer problem using
 *		pthreads.
 *
 *	Long 	:
 *
 *	Author	: Andrae Muys
 *
 *	Date	: 18 September 1997
 *
 *	Revised	: Panagiotis Avramidis, 2021
 */

#include <pthread.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define MAX_CLOSE_CONTACTS 5
#define TIMESCALE (uint64_t)100L
#define BYTE_MAX (1 << 8)
#define MAX_HIST (20*6) // 20 minutes, then forget
#define MACS_POOL 500
#define CLOSE_CONTACT_WAIT (14*24*60*60) // Forget after 14 days 
#define TEST_PERIOD ((4*60L*60L*((uint64_t) 1000000L) / TIMESCALE)) // Test every 4 hours
#define RUNTIME_MINUTES 30*24*60 // 30 DAYS

typedef uint64_t macaddress;

typedef struct btsearchobj
{
  macaddress mac;
  struct timeval start;
  struct timeval end;
} btsearchobj;

// 

typedef struct {
  btsearchobj *buf; // [MAX_CLOSE_CONTACTS];
  uint32_t head, tail;
  uint32_t full, empty, len;
  uint32_t dynamic;
  pthread_mutex_t *mut;
  pthread_cond_t *notFull, *notEmpty;
} queue;

typedef struct {

queue *MacSearches;
btsearchobj MacSearch;
queue *CloseContacts;
uint64_t *macs_pool;
uint32_t last_index;
bool     search_finished;
bool     cc_queue_notEmpty;
bool     terminate;
pthread_cond_t *Ready;
pthread_cond_t *cc_Ready;
} global_object;

queue *queueInit (int len, bool dynamic);
void queueDelete (queue *q);
void queueAdd (queue *q, btsearchobj in);
void queueDel (queue *q, btsearchobj *out);


uint64_t * gen_randmac_array();
macaddress BTnearMe(uint64_t *macs_pool);
bool testCOVID();

void uploadContacts(queue *cc_q);

void* bt_search_worker(void* global_arg);
void* close_contacts_worker(void* global_arg);
void* close_contacts_timer(void* global_arg);
void* test_covid_timer(void* global_arg);
uint64_t timediff(struct timeval time_past, uint64_t interval);
void printf_queue(queue q);
uint64_t tval_cvrt(struct timeval tval, int interval_sec);