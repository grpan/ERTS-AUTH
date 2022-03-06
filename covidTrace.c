/*
 *	File	: covidTrace.c
 *
 *	Title	: Covid contact tracing
 *
 *	Short	: A real-time programming assignment concerning featuring a contact traccing app.
 *  Simulates "fake bluetooth" scans to notify them in case of positive test result.
 *
 *	Author	: Panagiotis Avramidis
 *
 */

#include "covidTrace.h"
#include "queue.c"

#if UINTPTR_MAX == 0xffffffff
/* 32-bit */
#define pizero true
#elif UINTPTR_MAX == 0xffffffffffffffff
/* 64-bit */
#else
/* ??? */
#endif

//THREADS
pthread_t bt_search_worker_tid;
pthread_t close_contacts_worker_tid;
pthread_t close_contacts_timer_tid;
pthread_t test_covid_timer_tid;
//THREADS

//int main (int argc, char* argv[])
int main ()
{
  
  time_t t;
  srand((unsigned) time(&t));
  // srand((uint32_t) 8803951635056); // Make it deterministic for debugging purposes

  global_object *global_arg = (global_object*)malloc(sizeof(global_object));
  // global_arg->MacSearches = (btsearchobj*)malloc(MAX_HIST*sizeof(btsearchobj));
  global_arg->CloseContacts = queueInit(MAX_CLOSE_CONTACTS, 1);
  global_arg->MacSearches = queueInit(MAX_HIST, 0);

  global_arg->macs_pool = gen_randmac_array();
  global_arg->last_index = 0;
  global_arg->search_finished = false;
  global_arg->Ready = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  global_arg->cc_Ready = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  global_arg->terminate = false;
  pthread_cond_init(global_arg->cc_Ready, NULL);
  pthread_cond_init(global_arg->Ready, NULL);
  if (!global_arg->MacSearches) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  mkdir("log",0755);

  

  //////////////////////////////// Create thread here! ///////////////////////////////////
  pthread_create(&bt_search_worker_tid, NULL, bt_search_worker, (void*)global_arg);
  pthread_create(&close_contacts_worker_tid, NULL, close_contacts_worker, (void*)global_arg);
  pthread_create(&close_contacts_timer_tid, NULL, close_contacts_timer, (void*)global_arg);
  pthread_create(&test_covid_timer_tid, NULL, test_covid_timer, (void*)global_arg);
  //////////////////////////////// Create thread here! ///////////////////////////////////

  //////////////////////////////// Join thread here! ///////////////////////////////////
  pthread_join(bt_search_worker_tid, NULL);
  printf("Joined bt_search_worker_tid\n");
  pthread_join(close_contacts_worker_tid, NULL);
  printf("Joined close_contacts_worker_tid\n");
  pthread_join(close_contacts_timer_tid, NULL);
  printf("Joined close_contacts_timer_tid \n");
  pthread_join(test_covid_timer_tid, NULL);
  printf("Joined test_covid_timer_tid \n");
  //////////////////////////////// Join thread here! ///////////////////////////////////

  queueDelete(global_arg->CloseContacts);
  queueDelete(global_arg->MacSearches);
  free(global_arg->Ready);
  free(global_arg->cc_Ready);
  free(global_arg);
}

void* bt_search_worker(void* global_arg)
{
  global_object *g_arg = (global_object* )global_arg;

  queue *MacSearches = g_arg->MacSearches;
  struct btsearchobj *MacSearch = &g_arg->MacSearch;
  queue *cc_q = g_arg->CloseContacts;
  uint32_t *l_idx = &g_arg->last_index;
  pthread_cond_t *Ready = g_arg->Ready;
  uint64_t *macs_pool = g_arg->macs_pool;
  

  // struct timeval start;
  uint64_t i = 0;

  uint64_t sleep_time = 0;

  FILE *file_bt;
  file_bt = fopen("./log/bt_searches.bin", "wb");

  struct timeval btsearch_start;
  //Start the 10s scanning...
  gettimeofday(&btsearch_start, NULL);
  while(i<RUNTIME_MINUTES*6)
  {
    pthread_mutex_lock (MacSearches->mut);
    gettimeofday(&MacSearch->start, NULL);
    MacSearch->mac = BTnearMe(macs_pool);
    queueAdd(MacSearches, *MacSearch);
    
    // gettimeofday(&start, NULL);
      
    pthread_cond_signal (MacSearches->notEmpty);
    pthread_cond_signal (Ready);
    pthread_mutex_unlock (MacSearches->mut);
    *l_idx = i;

    fwrite(&(MacSearch->mac) , 1 , sizeof(MacSearch->mac) -2, file_bt);
    #ifndef pizero
      fwrite(&(MacSearch->start) , 1 , sizeof(MacSearch->start) -4, file_bt);
    #else
      fwrite(&(MacSearch->start) , 1 , sizeof(MacSearch->start), file_bt);
    #endif

    sleep_time = (i+1)*1000*((1000*10L/ TIMESCALE)) - ((MacSearch->start.tv_sec - btsearch_start.tv_sec)*1000000L + (MacSearch->start.tv_usec - btsearch_start.tv_usec));
    usleep( sleep_time );
    i++;
  }
  fclose(file_bt);
  g_arg->terminate = true;
  pthread_mutex_lock (cc_q->mut);
  pthread_cancel(close_contacts_worker_tid);
  // pthread_cancel(close_contacts_timer_tid);
  pthread_mutex_unlock (cc_q->mut);
  return NULL;
}

void* close_contacts_worker(void* global_arg)
{
  global_object *g_arg = (global_object* )global_arg;


  queue *MacSearches = g_arg->MacSearches;
  struct btsearchobj *MacSearch = &g_arg->MacSearch;

  queue *cc_q = g_arg->CloseContacts;

  pthread_cond_t *Ready = g_arg->Ready;

  struct timeval readydbg_s;
  struct timeval readydbg_e;

  while (true) {
    
    if(g_arg->terminate && cc_q->empty) {
      // fclose(file_cc);
      return NULL;
    }

    pthread_mutex_lock (MacSearches->mut);
    while (MacSearches->empty) {
      // printf ("[worker] MacSearches: queue EMPTY.\n");
      pthread_cond_wait (MacSearches->notEmpty, MacSearches->mut);
    }
    
    gettimeofday(&readydbg_s, NULL);
    pthread_cond_wait (Ready, MacSearches->mut);
    gettimeofday(&readydbg_e, NULL);

    // https://stackoverflow.com/questions/1907565/c-and-python-different-behaviour-of-the-modulo-operation
    for (int cnt=MacSearches->tail; cnt!= (int32_t) ((MacSearches->tail-1 % MAX_HIST) + MAX_HIST) % MAX_HIST; cnt = (cnt + 1) % MAX_HIST) {
      
      
      if (MacSearch->mac == MacSearches->buf[cnt].mac) {
        uint64_t timediff = ((MacSearch->start.tv_sec - MacSearches->buf[cnt].start.tv_sec)*1000000L + (MacSearch->start.tv_usec - MacSearches->buf[cnt].start.tv_usec));
      
        if (timediff > 4*60*1000*1000 / TIMESCALE) {

      
      

          pthread_mutex_lock (cc_q->mut);

          queueAdd(cc_q, *MacSearch);
          assert(MacSearches->buf[cnt].mac == MacSearch->mac);
          assert(cc_q->full != 1);
          
          pthread_mutex_unlock (cc_q->mut);
          pthread_cond_signal (cc_q->notEmpty);
          break;
        }
      }
    }
    pthread_mutex_unlock (MacSearches->mut);
  }
  return NULL;
}


void* close_contacts_timer(void* global_arg){
  global_object *g_arg = (global_object* )global_arg;

  queue *cc_q = g_arg->CloseContacts;

  btsearchobj delobj;
  struct timeval removed_time;
  struct timeval start_dbg_time;


  FILE *file_cc;
  file_cc = fopen("./log/close_contacts.bin", "wb");


  while(true) {
    
    if(g_arg->terminate && cc_q->empty) {
      fclose(file_cc);
      return NULL;
    }

    gettimeofday(&start_dbg_time, NULL);

    pthread_mutex_lock (cc_q->mut);
    while (cc_q->empty) {

      pthread_cond_wait (cc_q->notEmpty, cc_q->mut);
    }


    pthread_mutex_unlock (cc_q->mut);


    usleep(timediff(cc_q->buf[cc_q->head].start, CLOSE_CONTACT_WAIT));

    pthread_mutex_lock (cc_q->mut);
    queueDel (cc_q, &delobj);

    gettimeofday(&removed_time, NULL);

    pthread_mutex_unlock (cc_q->mut);
    


    fwrite(&(delobj.mac) , 1 , sizeof(delobj.mac) -2, file_cc);
    #ifndef pizero
      fwrite(&(removed_time) , 1 , sizeof(delobj.start) -4, file_cc);
    #else
      fwrite(&(removed_time) , 1 , sizeof(delobj.start), file_cc);
    #endif

  }
  return NULL;
}


void* test_covid_timer(void* global_arg){
  global_object *g_arg = (global_object* )global_arg;
  queue *cc_q = g_arg->CloseContacts;
  fclose(fopen("./log/uploaded_contacts.bin", "wb"));
  FILE *file_uc_cnt;
  file_uc_cnt = fopen("./log/uploaded_contacts_counter.bin", "wb");

  usleep(TEST_PERIOD);

  while(!g_arg->terminate) {

    uint16_t total_contacts = 0;
    struct timeval upload_time;

    if(testCOVID()) {
      printf("INFECTED! \n");
      gettimeofday(&upload_time, NULL);
      total_contacts = cc_q->tail - cc_q->head;

      fwrite(&(total_contacts) , 1 , sizeof(total_contacts), file_uc_cnt);
      
      #ifndef pizero
      fwrite(&(upload_time) , 1 , sizeof(upload_time) -4, file_uc_cnt);
      #else
      fwrite(&(upload_time) , 1 , sizeof(upload_time), file_uc_cnt);
      #endif
      uploadContacts(cc_q);
    }
    usleep(TEST_PERIOD);
  }

  fclose(file_uc_cnt);
  return NULL;
}

macaddress BTnearMe(uint64_t *macs_pool)
{
  macaddress mac;

  uint16_t mac_id = rand() % MACS_POOL;
  
  mac = macs_pool[mac_id]; // better
  
  return mac;
}


uint64_t gen_randmac()
{
  uint64_t mac = 0;

  uint64_t byte = 0;
  uint8_t i = 0;

  for (i=0;i<6;i++)
  {
    byte = rand() % BYTE_MAX;
    mac = mac | (byte << (i*8));
  }
  return mac;
}

uint64_t * gen_randmac_array()
{
  static uint64_t macs_pool[MACS_POOL];

  for (uint16_t i=0;i<MACS_POOL;i++)
  {
    macs_pool[i] = gen_randmac();
  }
  
  return macs_pool;
}

bool testCOVID()
{
  // https://stackoverflow.com/questions/33060893/whats-a-simple-way-to-generate-a-random-bool-in-c#comment53941717_33060909
  //return rand() > RAND_MAX/2;

  int r = rand();
  int division = RAND_MAX / 16;


  // return rand() < RAND_MAX/16;
  return r < division;
}

void uploadContacts(queue *cc_q)
{
  FILE *file_uc;
  file_uc = fopen("./log/uploaded_contacts.bin", "ab");
  pthread_mutex_lock(cc_q->mut);


    if (cc_q->empty == 1) {
      pthread_mutex_unlock(cc_q->mut);

      return;
  }

	uint32_t cnt = cc_q->head;
	do   {
    if (cc_q->len == cnt) {
      cnt = 0;
      if (cnt == cc_q->tail) {
        break;
      }
    }

    fwrite(&(cc_q->buf[cnt].mac) , 1 , sizeof(cc_q->buf[cnt].mac) -2, file_uc);
		cnt++;
	} while (cnt!=cc_q->tail );
  pthread_mutex_unlock(cc_q->mut);
  fclose(file_uc);
}

uint64_t timediff(struct timeval time_past, uint64_t interval)
{
  struct timeval current;
  uint64_t time_usec;
  gettimeofday(&current, NULL);
  time_usec = ((time_past.tv_sec+((interval*1.0) / TIMESCALE))*1000000L+time_past.tv_usec)-(current.tv_sec*1000000L+current.tv_usec);

  return time_usec ;
}

uint64_t tval_cvrt(struct timeval tval, int interval_sec)
{
  return ((long long int)tval.tv_sec + interval_sec)*1000000+tval.tv_usec;
}
