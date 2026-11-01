/* Host stand-ins for the EE kernel thread/semaphore calls used by STIO read-ahead.
 *
 * Threads are real pthreads and semaphores are POSIX semaphores, so the host
 * tests exercise genuine concurrency (including under ThreadSanitizer). They do
 * not model EE priority scheduling: a host thread may run where the EE would
 * not. Counting semaphores assert their max_count, as the EE kernel rejects an
 * over-signal. Set shim_fail_sema / shim_fail_thread to the 1-based call number
 * that should fail. */
#ifndef PS2_KERNEL_SHIM_H
#define PS2_KERNEL_SHIM_H

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#define SMS_THREAD_PRIORITY 64

typedef struct {
 int count, max_count, init_count, wait_threads;
 unsigned int attr, option;
} ee_sema_t;

typedef struct {
 int status;
 void ( *func ) ( void* );
 void* stack;
 int stack_size;
 void* gp_reg;
 int initial_priority;
 unsigned int attr, option;
} ee_thread_t;

void* _gp;

#define SHIM_MAX 32
static sem_t           shim_sem[ SHIM_MAX ];
static int             shim_sem_used[ SHIM_MAX ], shim_sem_max[ SHIM_MAX ];
static pthread_mutex_t shim_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned        shim_sema_calls, shim_thread_calls, shim_fail_sema, shim_fail_thread;
static unsigned        shim_threads_started;
static void          ( *shim_func[ SHIM_MAX ] ) ( void* );

static int CreateSema ( ee_sema_t* s ) {
 int i, r = -1;
 pthread_mutex_lock ( &shim_lock );
 if ( ++shim_sema_calls != shim_fail_sema ) {
  for ( i = 0; i < SHIM_MAX; ++i ) if ( !shim_sem_used[ i ] ) {
   assert ( s -> init_count >= 0 && s -> init_count <= s -> max_count );
   sem_init ( &shim_sem[ i ], 0, ( unsigned )s -> init_count );
   shim_sem_used[ i ] = 1; shim_sem_max[ i ] = s -> max_count; r = i;
   break;
  }
 }
 pthread_mutex_unlock ( &shim_lock );
 return r;
}

static int DeleteSema ( int id ) {
 pthread_mutex_lock ( &shim_lock );
 assert ( id >= 0 && id < SHIM_MAX && shim_sem_used[ id ] );
 sem_destroy ( &shim_sem[ id ] ); shim_sem_used[ id ] = 0;
 pthread_mutex_unlock ( &shim_lock );
 return id;
}

static int WaitSema ( int id ) {
 assert ( id >= 0 && id < SHIM_MAX && shim_sem_used[ id ] );
 while ( sem_wait ( &shim_sem[ id ] ) != 0 ) assert ( errno == EINTR );
 return id;
}

static int SignalSema ( int id ) {
 int v;
 assert ( id >= 0 && id < SHIM_MAX && shim_sem_used[ id ] );
 sem_getvalue ( &shim_sem[ id ], &v );
 assert ( v < shim_sem_max[ id ] );
 sem_post ( &shim_sem[ id ] );
 return id;
}

static int shim_sems_in_use ( void ) {
 int i, n = 0;
 for ( i = 0; i < SHIM_MAX; ++i ) n += shim_sem_used[ i ];
 return n;
}

static void* shim_entry ( void* p ) {
 void ( *f ) ( void* ) = ( void ( * ) ( void* ) )p;
 f ( NULL );
 return NULL;
}

static int CreateThread ( ee_thread_t* t ) {
 int i;
 assert ( t -> func && t -> stack && t -> stack_size > 0 && t -> gp_reg == &_gp );
 assert ( t -> initial_priority < SMS_THREAD_PRIORITY );   /* above the main/decoder threads */
 if ( ++shim_thread_calls == shim_fail_thread ) return -1;
 for ( i = 1; i < SHIM_MAX; ++i ) if ( !shim_func[ i ] ) { shim_func[ i ] = t -> func; return i; }
 return -1;
}

static int StartThread ( int id, void* arg ) {
 pthread_t th;
 ( void )arg;
 assert ( id > 0 && id < SHIM_MAX && shim_func[ id ] );
 if ( pthread_create ( &th, NULL, shim_entry, ( void* )shim_func[ id ] ) ) return -1;
 pthread_detach ( th );
 ++shim_threads_started;
 return id;
}

static int DeleteThread ( int id ) {
 assert ( id > 0 && id < SHIM_MAX );
 shim_func[ id ] = NULL;
 return id;
}

#endif /* PS2_KERNEL_SHIM_H */
