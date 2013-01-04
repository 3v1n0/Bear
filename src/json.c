// This file is distributed under MIT-LICENSE. See COPYING for details.

#include "json.h"
#include "stringarray.h"

#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

static char const * fix_single_argument(char const *);

char const * json_escape(char const * * raw) {
    char const * * it = raw;
    for (; (raw) && (*it); ++it) {
        char const * const new = fix_single_argument(*it);
        if (new) {
            char const * const tmp = *it;
            *it = new;
            free((void *)tmp);
        }
    }
    return sa_fold(raw, ' ');
}

static size_t count(char const * const begin,
                    char const * const end,
                    int(*fp)(int));

static int needs_escape(int);

static char const * fix_single_argument(char const * raw) {
    size_t const length = (raw) ? strlen(raw) : 0;
    size_t const spaces = count(raw, raw + length, isspace);
    size_t const json = count(raw, raw + length, needs_escape);

    if ((0 == spaces) && (0 == json)) {
        return 0;
    }

    char * const result = malloc(length + ((0 != spaces) * 4) + json + 1);
    if (0 == result) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    char * it = result;
    if (spaces) {
        *it++ = '\\';
        *it++ = '\"';
    }
    for (; (raw) && (*raw); ++raw) {
        if (needs_escape(*raw)) {
            *it++ = '\\';
        }
        *it++ = *raw;
    }
    if (spaces) {
        *it++ = '\\';
        *it++ = '\"';
    }
    *it = '\0';
    return result;
}

static size_t count(char const * const begin,
                    char const * const end,
                    int (*fp)(int)) {
    size_t result = 0;
    char const * it = begin;
    for (; it != end; ++it) {
        if (fp(*it)) {
            ++result;
        }
    }
    return result;
}

static int needs_escape(int c) {
    switch (c) {
    case '\\':
    case '\"':
        return 1;
    }
    return 0;
}

