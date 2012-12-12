// This file is distributed under MIT-LICENSE. See COPYING for details.

#include "stringarray.h"

#include <malloc.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

Strings sa_copy(Strings const in) {
    size_t const size = sa_length(in);

    Strings result = (Strings)malloc((size + 1) * sizeof(String));
    if (0 == result) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    result[size] = 0;

    char const * const * in_it = in;
    char const * * out_it = result;
    for (;*in_it;++in_it,++out_it) {
        *out_it = strdup(*in_it);
        if (0 == *out_it) {
            perror("strdup");
            exit(EXIT_FAILURE);
        }
    }
    return result;
}

Strings sa_append(Strings const in, String e) {
    if (0 == e) {
        return in;
    }
    size_t size = sa_length(in);
    Strings result = (Strings)realloc(in, (size + 2) * sizeof(String));
    if (0 == result) {
        perror("realloc");
        exit(EXIT_FAILURE);
    }
    result[size++] = e;
    result[size++] = 0;
    return result;
}

void sa_release(Strings in) {
    char const * const * it = in;
    for (; (in) && (*it); ++it) {
        free((void *)*it);
    }
    free((void *)in);
    in = 0;
}

size_t sa_length(Strings const in) {
    size_t result = 0;
    char const * const * it = in;
    for (; (in) && (*it); ++it, ++result)
        ;
    return result;
}

int sa_find(Strings const in, String e) {
    if (0 == e)
        return 0;

    char const * const * it = in;
    for (; (in) && (*it); ++it) {
        if (0 == strcmp(e, *it)) {
            return 1;
        }
    }
    return 0;
}

String sa_fold(Strings const in, char const separator) {
    char * acc = 0;
    size_t acc_size = 0;

    char const * const * it = in;
    for (;*it;++it) {
        size_t const sep = (in == it) ? 0 : 1;
        size_t const it_size = strlen(*it);
        acc = (char *)realloc(acc, acc_size + sep + it_size);
        if (0 == acc) {
            perror("realloc");
            exit(EXIT_FAILURE);
        }
        if (sep) {
            acc[acc_size++] = separator;
        }
        strncpy((acc + acc_size), *it, it_size);
        acc_size += it_size;
    }
    acc = (char *)realloc(acc, acc_size + 1);
    if (0 == acc) {
        perror("realloc");
        exit(EXIT_FAILURE);
    }
    acc[acc_size++] = '\0';
    return acc;
}
