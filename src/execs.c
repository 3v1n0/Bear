// This file is distributed under MIT-LICENSE. See COPYING for details.

#include "report.h"
#include "stringarray.h"
#include "envarray.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <dlfcn.h>


int execve(const char *path, char *const argv[], char *const envp[]) {
    report_call("execve", argv);

    int (*fp)(const char *, char *const *, char *const *) = 0;
    if (0 == (fp = dlsym(RTLD_NEXT, "execve"))) {
        perror("dlsym");
        exit(EXIT_FAILURE);
    }

    Strings menvp = sa_copy((Strings)envp);
    menvp = env_insert(menvp, ENV_PRELOAD, getenv(ENV_PRELOAD));
    menvp = env_insert(menvp, ENV_OUTPUT, getenv(ENV_OUTPUT));
    int const result = (*fp)(path, argv, (char *const *)menvp);
    sa_release(menvp);
    return result;
}

int execv(const char *path, char *const argv[]) {
    return execve(path, argv, environ);
}

int execvpe(const char *file, char *const argv[], char *const envp[]) {
    report_call("execvpe", argv);

    int (*fp)(const char *, char *const *, char *const *) = 0;
    if (0 == (fp = dlsym(RTLD_NEXT, "execvpe"))) {
        perror("dlsym");
        exit(EXIT_FAILURE);
    }

    Strings menvp = sa_copy((Strings)envp);
    menvp = env_insert(menvp, ENV_PRELOAD, getenv(ENV_PRELOAD));
    menvp = env_insert(menvp, ENV_OUTPUT, getenv(ENV_OUTPUT));
    int const result = (*fp)(file, argv, (char *const *)menvp);
    sa_release(menvp);
    return result;
}

int execvp(const char *file, char *const argv[]) {
    return execvpe(file, argv, environ);
}

int execl(const char *path, const char *arg, ...) {
    va_list args;
    va_start(args, arg);
    Strings argv = sa_build(arg, args);
    va_end(args);

    int const result = execve(path, (char * const *)argv, environ);
    sa_release(argv);
    return result;
}

int execlp(const char *file, const char *arg, ...) {
    va_list args;
    va_start(args, arg);
    Strings argv = sa_build(arg, args);
    va_end(args);

    int const result = execvpe(file, (char * const *)argv, environ);
    sa_release(argv);
    return result;
}

// int execle(const char *path, const char *arg, ..., char * const envp[]);
int execle(const char *path, const char *arg, ...) {
    va_list args;
    va_start(args, arg);
    Strings argv = sa_build(arg, args);
    Strings envp = va_arg(args, Strings);
    va_end(args);

    int const result = execve(path, (char *const *)argv, (char *const *)envp);
    sa_release(argv);
    return result;
}

