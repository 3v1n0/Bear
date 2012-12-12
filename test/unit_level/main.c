// This file is distributed under MIT-LICENSE. See COPYING for details.

#include <assert.h>

#include <malloc.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include <stringarray.h>
#include <protocol.h>
#include <json.h>

void test_sa_length() {
    char const * input[] =
        { "this"
        , "is"
        , "my"
        , "message"
        , 0
        };
    assert(4 == sa_length(input));
    assert(2 == sa_length(input + 2));
    assert(0 == sa_length(0));
}

void test_sa_fold() {
    char const * input[] =
        { "this"
        , "is"
        , "my"
        , "message"
        , 0
        };
    char const * const expected = "this\x1fis\x1fmy\x1fmessage";
    char const * const result = sa_fold(input, '\x1f');

    assert((0 == strcmp(expected, result)) && "sa_fold failed");

    free((void *)result);
}

void test_sa_append() {
    Strings result = 0;

    result = sa_append(result, strdup("this"));

    assert(1 == sa_length(result));
    assert(0 == strcmp("this", result[0]));
    assert(0 == result[1]);

    result = sa_append(result, strdup("that"));

    assert(2 == sa_length(result));
    assert(0 == strcmp("this", result[0]));
    assert(0 == strcmp("that", result[1]));
    assert(0 == result[2]);

    sa_release(result);
}

void test_sa_find() {
    char const * input[] =
        { "this"
        , "is"
        , "my"
        , "message"
        , 0
        };
    assert(sa_find(input, "this"));
    assert(sa_find(input, "my"));

    assert(0 == sa_find(input, "th"));
    assert(0 == sa_find(input, "messa"));
}

void test_sa_copy() {
    char const * input[] =
        { "this"
        , "is"
        , "my"
        , "message"
        , 0
        };
    Strings result = sa_copy(input);

    assert(4 == sa_length(result));
    assert(0 == strcmp("this",      result[0]));
    assert(0 == strcmp("is",        result[1]));
    assert(0 == strcmp("my",        result[2]));
    assert(0 == strcmp("message",   result[3]));
    assert(0 == result[4]);

    sa_release(result);
}

void test_string_io() {
    int fds[2];
    pipe(fds);

    char const * const in_msg_1 = "this is my fmessage!";
    char const * const in_msg_2 = "";

    write_string(fds[1], in_msg_1);
    write_string(fds[1], in_msg_2);
    write_string(fds[1], 0);

    char const * const out_msg_1 = read_string(fds[0]);
    char const * const out_msg_2 = read_string(fds[0]);
    char const * const out_msg_3 = read_string(fds[0]);

    assert(0 == strcmp(in_msg_1, out_msg_1));
    assert(0 == strcmp(in_msg_2, out_msg_2));
    assert(0 == strcmp(in_msg_2, out_msg_3));

    free((void *)out_msg_1);
    free((void *)out_msg_2);
    free((void *)out_msg_3);
}

void test_string_array_io() {
    int fds[2];
    pipe(fds);

    char const * input[] =
        { "this"
        , "is"
        , "my"
        , "message"
        , 0
        };

    write_string_array(fds[1], input);

    char const * * const result = read_string_array(fds[0]);

    assert(4 == sa_length(result));
    assert(0 == strcmp("this",      result[0]));
    assert(0 == strcmp("is",        result[1]));
    assert(0 == strcmp("my",        result[2]));
    assert(0 == strcmp("message",   result[3]));
    assert(0 == result[4]);

    sa_release(result);
}

void test_json() {
    char const * input_const[] =
        { "this"
        , "is my"
        , "message=\"shit\\gold\""
        , 0
        };
    Strings input = sa_copy(input_const);
    String result = json_escape(input);

    assert(0 == strcmp("this \\\"is my\\\" message=\\\"shit\\\\gold\\\"",
                       result));

    sa_release(input);
    free((void *)result);
}

int main() {
    test_sa_length();
    test_sa_fold();
    test_sa_append();
    test_sa_find();
    test_sa_copy();
    test_string_io();
    test_string_array_io();
    test_json();
    return 0;
}
