// This file is distributed under MIT-LICENSE. See COPYING for details.

#include "protocol.h"
#include "output.h"
#include "environ.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>

// Stringify environment variables
#define XSTR(s) STR(s)
#define STR(s) #s

#define SOCKET_FILE XSTR(DEFAULT_SOCKET_FILE)
#define OUTPUT_FILE XSTR(DEFAULT_OUTPUT_FILE)
#define LIBEAR_FILE XSTR(DEFAULT_PRELOAD_FILE)

// variables which are used in signal handler
static pid_t    child_pid;
static int      child_status = EXIT_FAILURE;

static void usage(char const * const name)  __attribute__ ((noreturn));
static void mask_all_signals(int command);
static void install_signal_handler(int signum);
static void collect_messages(char const * socket, char const * output, int debug);


int main(int argc, char * const argv[]) {
    char const * socket_file = SOCKET_FILE;
    char const * output_file = OUTPUT_FILE;
    char const * libear_path = LIBEAR_FILE;
    int debug = 0;
    char * const * unprocessed_argv = 0;
    // parse command line arguments.
    int flags, opt;
    while ((opt = getopt(argc, argv, "o:b:s:d")) != -1) {
        switch (opt) {
        case 'o':
            output_file = optarg;
            break;
        case 'b':
            libear_path = optarg;
            break;
        case 's':
            socket_file = optarg;
            break;
        case 'd':
            debug = 1;
            break;
        default: /* '?' */
            usage(argv[0]);
        }
    }
    // validate
    if (argc == optind) {
        usage(argv[0]);
    }
    unprocessed_argv = &(argv[optind]);
    // fork
    child_pid = fork();
    if (-1 == child_pid) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (0 == child_pid) {
        // child process
        if (-1 == setenv(ENV_PRELOAD, libear_path, 1)) {
            perror("setenv");
            exit(EXIT_FAILURE);
        }
        if (-1 == setenv(ENV_OUTPUT, socket_file, 1)) {
            perror("setenv");
            exit(EXIT_FAILURE);
        }
        if (-1 == execvp(*unprocessed_argv, unprocessed_argv)) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    } else {
        // parent process
        install_signal_handler(SIGCHLD);
        install_signal_handler(SIGINT);
        mask_all_signals(SIG_BLOCK);
        collect_messages(socket_file, output_file, debug);
    }
    return child_status;
}

static void receive_on_unix_socket(char const * socket_file, int output_fd, int debug);

static void collect_messages(char const * socket_file, char const * output_file, int debug) {
    // open the output file
    int output_fd = bear_open_json_output(output_file);
    // remove old socket file if any
    if ((-1 == unlink(socket_file)) && (ENOENT != errno)) {
        perror("unlink");
        exit(EXIT_FAILURE);
    }
    // receive messages
    receive_on_unix_socket(socket_file, output_fd, debug);
    // skip errors during shutdown
    bear_close_json_output(output_fd);
    unlink(socket_file);
}

static int create_unix_socket(char const * file);
static int accept_message(int fd, struct bear_message * msg);

static void receive_on_unix_socket(char const * file, int out_fd, int debug) {
    int s = create_unix_socket(file);
    mask_all_signals(SIG_UNBLOCK);
    struct bear_message msg = { 0, 0, 0, 0 };
    while (accept_message(s, &msg)) {
        mask_all_signals(SIG_BLOCK);
        bear_append_json_output(out_fd, &msg, debug);
        bear_free_message(&msg);
        mask_all_signals(SIG_UNBLOCK);
    }
    mask_all_signals(SIG_BLOCK);
    close(s);
}

static int create_unix_socket(char const * file) {
    struct sockaddr_un addr;
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (-1 == s) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, file, sizeof(addr.sun_path) - 1);
    if (-1 == bind(s, (struct sockaddr *)&addr, sizeof(struct sockaddr_un))) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    if (-1 == listen(s, 0)) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    return s;
}

static int accept_message(int s, struct bear_message * msg) {
    int connection = accept(s, 0, 0);
    if (-1 != connection) {
        bear_read_message(connection, msg);
        close(connection);
        return 1;
    }
    return 0;
}

static void handler(int signum) {
    switch (signum) {
    case SIGCHLD: {
        int status;
        while (0 > waitpid(WAIT_ANY, &status, WNOHANG)) ;
        child_status = WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE;
        break;
        }
    case SIGINT:
        kill(child_pid, signum);
    default:
        break;
    }
}

static void install_signal_handler(int signum) {
    struct sigaction action, old_action;
    sigemptyset(&action.sa_mask);
    action.sa_handler = handler;
    action.sa_flags = 0;
    if (-1 == sigaction(signum, &action, &old_action)) {
        perror( "sigaction");
        exit(EXIT_FAILURE);
    }
}

static void mask_all_signals(int command) {
    sigset_t signal_mask;
    sigfillset(&signal_mask);
    if (-1 == sigprocmask(command, &signal_mask, 0)) {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }
}

static void usage(char const * const name) {
    fprintf(stderr,
            "Usage: %s [-o output] [-b libear] [-d socket] -- command\n"
            "\n"
            "   -o output   output file (default: %s)\n"
            "   -b libear   libear.so location (default: %s)\n"
            "   -s socket   multiplexing socket (default: %s)\n"
            "   -d          debug output (default: disabled)\n",
            name,
            OUTPUT_FILE,
            LIBEAR_FILE,
            SOCKET_FILE);
    exit(EXIT_FAILURE);
}

