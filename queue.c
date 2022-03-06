// #include <stdbool.h>
// #include <stdint.h>
// #include <stdio.h>
// #include <unistd.h>
#include <stdlib.h>
// #include <stdio.h>
#include <string.h>
// #include "covidTrace.h"

void printf_queue(queue q)
{
  uint32_t elems = q.tail - q.head;
  // printf(" (with tail: %ld and head: %ld)", q.tail, q.head);
  if (q.full==1) {
    elems = MAX_HIST;
    printf("queue full! extending elems.(size %d)", q.len);
  }

  printf("(h, t) :(%d, %d)\n", q.head, q.tail);

  printf("\n");
  for (uint32_t i=0;i<elems;i++) {

    printf("i: %d, MAC: %.12" PRIX64 ", added time: %" PRId64 " \n", i, q.buf[q.head + i].mac, (int64_t)(q.buf[q.head + i].start.tv_sec*1000000+q.buf[q.head + i].start.tv_usec));
  }
  
}

queue *queueInit (int len, bool dynamic)
{
  queue *q;
  
  q = (queue *)malloc (sizeof (queue));
  if (q == NULL) return (NULL);
  printf("Initialising queue with %d and dynamic: %d \n", len, dynamic);
  q->dynamic = dynamic;
  q->len = len;
  q->buf = (btsearchobj*)calloc(q->len , sizeof(btsearchobj));
  // q->buf = (btsearchobj*)malloc(q->len * sizeof(btsearchobj));
  q->empty = 1;
  q->full = 0;
  q->head = 0;
  q->tail = 0;
  q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
  pthread_mutex_init (q->mut, NULL);
  q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notFull, NULL);
  q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notEmpty, NULL);
	
  return (q);
}

void queueDelete (queue *q)
{
  pthread_mutex_destroy (q->mut);
  free (q->mut);	
  pthread_cond_destroy (q->notFull);
  free (q->notFull);
  pthread_cond_destroy (q->notEmpty);
  free (q->notEmpty);
  free(q->buf);
  free (q);
}

void queueAdd (queue *q, btsearchobj in)
{
  if (q->dynamic) {

    q->buf[q->tail] = in;
    q->tail++;

    if (q->tail==q->len) {

      q->len = q->len << 1;


      btsearchobj *temp;
      temp = realloc(q->buf, q->len * sizeof(btsearchobj));

      if (!temp) { printf("realloc error! \n"); exit(EXIT_FAILURE); }
      q->buf = temp;
      
      for (uint32_t i = q->tail; i<=q->len-1;i++) {
        q->buf[i].mac = 0;
        q->buf[i].start.tv_sec = 0;
        q->buf[i].start.tv_usec = 0;
      }
    }

      q->empty = 0;
      return;
  }

  q->buf[q->tail] = in;

  q->tail++;
  if (q->tail == q->len)
    q->tail = 0;
  if (q->tail == q->head)
    q->full = 1;
  q->empty = 0;

  return;
}

void queueDel (queue *q, btsearchobj *out)
{
  if (q->dynamic) {

    const uint32_t offset = 100; // Maximum number of free elements to the left.
    *out = q->buf[q->head];
    q->head++;
    if (q->head > offset) {


      memmove(q->buf, q->buf + offset, sizeof(btsearchobj)*(q->tail - offset));
      
      q->head -= offset;
      q->tail -= offset;
      for (uint32_t i = q->tail; i<=q->tail + offset;i++) {
        q->buf[i].mac = 0;
        q->buf[i].start.tv_sec = 0;
        q->buf[i].start.tv_usec = 0;
      }

    }
    if (q->head == q->tail) {

      q->empty = 1;
    }

    return;
  }

  *out = q->buf[q->head];

  q->head++;
  if (q->head == q->len)
    q->head = 0;
  if (q->head == q->tail)
    q->empty = 1;
  q->full = 0;

  return;
}