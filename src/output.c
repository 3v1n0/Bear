// This file is distributed under MIT-LICENSE. See COPYING for details.

#include "output.h"
#include "stringarray.h"
#include "protocol.h"
#include "json.h"

#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stddef.h>


static size_t count = 0;

int bear_open_json_output(char const * file) {
    int fd = open(file, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
    if (-1 == fd) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    dprintf(fd, "[\n");
    count = 0;
    return fd;
}

void bear_close_json_output(int fd) {
    dprintf(fd, "]\n");
    close(fd);
}

static char const * get_source_file(char const * * cmd, char const * cwd);

void bear_append_json_output(int fd, struct bear_message const * e, int debug) {
    char const * src = get_source_file(e->cmd, e->cwd);
    char const * const cmd = bear_strings_fold(bear_json_escape_strings(e->cmd), ' ');
    if (src) {
        if (count++) {
            dprintf(fd, ",\n");
        }
        dprintf(fd, "{\n"
                    "  \"directory\": \"%s\",\n"
                    "  \"command\": \"%s\",\n"
                    "  \"file\": \"%s\"\n"
                    "}\n", e->cwd, cmd, src);
    } else if (debug) {
        dprintf(fd, "{\n"
                    "  \"directory\": \"%s\",\n"
                    "  \"command\": \"%s\"\n"
                    "}\n", e->cwd, cmd);
    }
    free((void *)cmd);
    free((void *)src);
}


static int is_known_compiler(char const * cmd);
static int is_source_file(char const * const arg);

static char const * fix_path(char const * file, char const * cwd);


static char const * get_source_file(char const * * args, char const * cwd) {
    char const * result = 0;
    // looking for compiler name
    if ((args) && (args[0]) && is_known_compiler(args[0])) {
        // looking for source file
        char const * const * it = args;
        for (; *it; ++it) {
            if (is_source_file(*it)) {
                result = fix_path(*it, cwd);
                break;
            }
        }
    }
    return result;
}

static char const * fix_path(char const * file, char const * cwd) {
    char * result = 0;
    if ('/' == file[0]) {
        result = strdup(file);
        if (0 == result) {
            perror("strdup");
            exit(EXIT_FAILURE);
        }
    } else {
        if (-1 == asprintf(&result, "%s/%s", cwd, file)) {
            perror("asprintf");
            exit(EXIT_FAILURE);
        }
    }
    return result;
}

static int is_known_compiler(char const * cmd) {
    static char const * compilers[] =
        { "cc"
        , "gcc"
        , "llvm-gcc"
        , "clang"
        , "c++"
        , "g++"
        , "llvm-g++"
        , "clang++"
        , 0
        };

    // looking for compiler name
    char * file = basename(cmd);

    return bear_strings_find(compilers, file);
}

static int is_source_file_extension(char const * arg);

static int is_source_file(char const * const arg) {
    char const * file_name = strrchr(arg, '/');
    file_name = (file_name) ? file_name : arg;
    char const * extension = strrchr(file_name, '.');
    extension = (extension) ? extension : file_name;

    return is_source_file_extension(extension);
}

static int is_source_file_extension(char const * arg) {
    static char const * extensions[] =
        { ".c"
        , ".C"
        , ".cc"
        , ".cxx"
        , ".c++"
        , ".C++"
        , ".cpp"
        , ".cp"
        , ".i"
        , ".ii"
        , ".m"
        , ".S"
        , 0
        };

    return bear_strings_find(extensions, arg);
}

