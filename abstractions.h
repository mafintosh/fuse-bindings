#include <nan.h>

#define FUSE_USE_VERSION 29

#ifdef __APPLE__

// OS X
#include <sys/mount.h>

#include <semaphore.h>
#include <dispatch/dispatch.h>

#include <fuse_lowlevel.h>

#define FUSE_OFF_T off_t

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

typedef pthread_t abstr_thread_t;
typedef void* thread_fn_rtn_t;

#elif defined(_WIN32)

#include <windows.h>
#include <io.h>
#include <winsock2.h>

typedef HANDLE bindings_sem_t;

NAN_INLINE static int semaphore_init (HANDLE *sem) {
  *sem = CreateSemaphore(NULL, 0, 10, NULL);
  return *sem == NULL ? -1 : 0;
}

NAN_INLINE static void semaphore_wait (HANDLE *sem) {
  WaitForSingleObject(*sem, INFINITE);
}

NAN_INLINE static void semaphore_signal (HANDLE *sem) {
  ReleaseSemaphore(*sem, 1, NULL);
}

extern HANDLE mutex;

NAN_INLINE static void mutex_lock (HANDLE *mutex) {
    WaitForSingleObject(*mutex, INFINITE);
}

NAN_INLINE static void mutex_unlock (HANDLE *mutex) {
    ReleaseMutex(*mutex);
}

typedef HANDLE abstr_thread_t;
typedef DWORD thread_fn_rtn_t;

#define fuse_session_remove_chan(x)
#define stat _stati64

#else

// Linux and whatnot
#include <sys/mount.h>

#include <semaphore.h>
#include <fuse_lowlevel.h>

#define FUSE_OFF_T off_t

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

typedef pthread_t abstr_thread_t;
typedef void* thread_fn_rtn_t;

#endif

typedef thread_fn_rtn_t(*thread_fn)(void*);

void thread_create (abstr_thread_t*, thread_fn, void*);
void thread_join (abstr_thread_t);

int fusermount (char*);
