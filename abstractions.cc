#include "abstractions.h"

#include <unistd.h>
#include <sys/wait.h>

#ifdef __APPLE__

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void thread_create (thread_t* thread, thread_fn fn, void* data) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(thread, &attr, fn, data);
}

void thread_join (thread_t thread) {
    pthread_join(thread, NULL);
}

void fusermount (char *path) {
    char *argv[] = {(char *) "umount", path, NULL};

    pid_t cpid = vfork();
    if (cpid > 0) waitpid(cpid, NULL, 0);
    else execvp(argv[0], argv);
}

#else

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void thread_create (thread_t* thread, thread_fn fn, void* data) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(thread, &attr, fn, data);
}

void thread_join (thread_t thread) {
    pthread_join(thread, NULL);
}

void fusermount (char *path) {
    char *argv[] = {(char *) "fusermount", (char *) "-q", (char *) "-u", path, NULL};

    pid_t cpid = vfork();
    if (cpid > 0) waitpid(cpid, NULL, 0);
    else execvp(argv[0], argv);
}

#endif
