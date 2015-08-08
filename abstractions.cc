#include "abstractions.h"

#ifdef __APPLE__

#include <unistd.h>
#include <sys/wait.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void thread_create (abstr_thread_t* thread, thread_fn fn, void* data) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(thread, &attr, fn, data);
}

void thread_join (abstr_thread_t thread) {
    pthread_join(thread, NULL);
}

void fusermount (char *path) {
    char *argv[] = {(char *) "umount", path, NULL};

    pid_t cpid = vfork();
    if (cpid > 0) waitpid(cpid, NULL, 0);
    else execvp(argv[0], argv);
}

#elif defined(_WIN32)

HANDLE mutex = CreateMutex(NULL, false, NULL);

void thread_create (HANDLE* thread, thread_fn fn, void* data) {
    *thread = CreateThread(NULL, 0, fn, data, 0, NULL);
}

void thread_join (HANDLE thread) {
    WaitForSingleObject(thread, INFINITE);
}

void fusermount (char *path) {
    char* dokanPath = getenv("DOKAN_INSTALL_DIR");
    char cmdLine[MAX_PATH];

    if(dokanPath) sprintf(cmdLine, "\"%s/dokanctl.exe\" /u %s", dokanPath, path);
    else sprintf(cmdLine, "dokanctl.exe /u %s", path);

    STARTUPINFO info = {sizeof(info)};
    PROCESS_INFORMATION procInfo;
    CreateProcess(NULL, cmdLine, NULL, NULL, false, CREATE_NO_WINDOW, NULL, NULL, &info, &procInfo);

    WaitForSingleObject(procInfo.hProcess, INFINITE);
    CloseHandle(procInfo.hProcess);
    CloseHandle(procInfo.hThread);

    // dokanctl.exe requires admin permissions for some reason, so if node is not run as admin,
    // it'll fail to create the process for unmounting. The path will be unmounted once
    // the process is killed, however, so there's that!
}

#else

#include <unistd.h>
#include <sys/wait.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void thread_create (abstr_thread_t* thread, thread_fn fn, void* data) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(thread, &attr, fn, data);
}

void thread_join (abstr_thread_t thread) {
    pthread_join(thread, NULL);
}

void fusermount (char *path) {
    char *argv[] = {(char *) "fusermount", (char *) "-q", (char *) "-u", path, NULL};

    pid_t cpid = vfork();
    if (cpid > 0) waitpid(cpid, NULL, 0);
    else execvp(argv[0], argv);
}

#endif
