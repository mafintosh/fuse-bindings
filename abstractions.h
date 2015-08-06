#include <nan.h>

typedef void*(*thread_fn)(void*);

#ifdef __APPLE__

// OS X
#include <semaphore.h>
#include <dispatch/dispatch.h>

typedef dispatch_semaphore_t bindings_sem_t;

NAN_INLINE static int semaphore_init (dispatch_semaphore_t *sem) {
  *sem = dispatch_semaphore_create(0);
  return *sem == NULL ? -1 : 0;
}

NAN_INLINE static void semaphore_wait (dispatch_semaphore_t *sem) {
  dispatch_semaphore_wait(*sem, DISPATCH_TIME_FOREVER);
}

NAN_INLINE static void semaphore_signal (dispatch_semaphore_t *sem) {
  dispatch_semaphore_signal(*sem);
}

extern pthread_mutex_t mutex;

NAN_INLINE static void mutex_lock (pthread_mutex_t *mutex) {
    pthread_mutex_lock(mutex);
}

NAN_INLINE static void mutex_unlock (pthread_mutex_t *mutex) {
    pthread_mutex_unlock(mutex);
}

typedef pthread_t thread_t;

void thread_create (thread_t*, thread_fn, void*);
void thread_join (thread_t);

#else

// Linux and whatnot
#include <semaphore.h>

typedef sem_t bindings_sem_t;

NAN_INLINE static int semaphore_init (sem_t *sem) {
  return sem_init(sem, 0, 0);
}

NAN_INLINE static void semaphore_wait (sem_t *sem) {
  sem_wait(sem);
}

NAN_INLINE static void semaphore_signal (sem_t *sem) {
  sem_post(sem);
}

extern pthread_mutex_t mutex;

NAN_INLINE static void mutex_lock (pthread_mutex_t *mutex) {
    pthread_mutex_lock(mutex);
}

NAN_INLINE static void mutex_unlock (pthread_mutex_t *mutex) {
    pthread_mutex_unlock(mutex);
}

typedef pthread_t thread_t;

void thread_create (thread_t*, thread_fn, void*);
void thread_join (thread_t);

void fusermount (char*);

#endif
