#include "abstractions.h"

#ifndef _WIN32

#include <unistd.h>
#include <sys/wait.h>

int execute_command_and_wait (char* argv[]) {
    // Fork our running process.
    pid_t cpid = vfork();

    // Check if we are the observer or the new process.
    if (cpid > 0) {
        int status = 0;
        waitpid(cpid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    } else {
        // At this point we are on our child process.
        execvp(argv[0], argv);
        exit(1);

        // Something failed.
        return -1;
    }
}

#endif

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

int fusermount (char *path) {
    char *argv[] = {(char *) "umount", path, NULL};

    return execute_command_and_wait(argv);
}

#elif defined(_WIN32)

HANDLE mutex = CreateMutex(NULL, false, NULL);

void thread_create (HANDLE* thread, thread_fn fn, void* data) {
    *thread = CreateThread(NULL, 0, fn, data, 0, NULL);
}

void thread_join (HANDLE thread) {
    WaitForSingleObject(thread, INFINITE);
}

int fusermount (char *path) {
    char* dokanPath = getenv("DokanLibrary1");
    char cmdLine[MAX_PATH];

    if(dokanPath) {
        // Let's make sure there aren't no double slashes
        const char* dokanPathLast = dokanPath + strlen(dokanPath) - 1;

        const char* potentialEndSlash =
            (*dokanPathLast == '/' || *dokanPathLast == '\\') ? "" : "\\";

        sprintf(cmdLine, "\"%s%sdokanctl.exe\" /u %s", dokanPath, potentialEndSlash, path);
    }
    else sprintf(cmdLine, "dokanctl.exe /u %s", path);

    STARTUPINFO info = {sizeof(info)};
    PROCESS_INFORMATION procInfo;
    CreateProcess(NULL, cmdLine, NULL, NULL, false, CREATE_NO_WINDOW, NULL, NULL, &info, &procInfo);

    WaitForSingleObject(procInfo.hProcess, INFINITE);

    DWORD exitCode = -1;
    GetExitCodeProcess(procInfo.hProcess, &exitCode);

    CloseHandle(procInfo.hProcess);
    CloseHandle(procInfo.hThread);

    return exitCode;

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

int fusermount (char *path) {
    char *argv[] = {(char *) "fusermount", (char *) "-q", (char *) "-u", path, NULL};

    return execute_command_and_wait(argv);
}

#endif
