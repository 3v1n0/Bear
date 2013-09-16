/*  Copyright (C) 2012, 2013 by László Nagy
    This file is part of Bear.

    Bear is a tool to generate compilation database for clang tooling.

    Bear is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Bear is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "protocol.h"
#include "output.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>

static char const * compilers[] =
{
    "cc",
    "gcc",
    "gcc-4.1",
    "gcc-4.2",
    "gcc-4.3",
    "gcc-4.4",
    "gcc-4.5",
    "gcc-4.6",
    "gcc-4.7",
    "gcc-4.8",
    "llvm-gcc",
    "clang",
    "clang-3.0",
    "clang-3.1",
    "clang-3.2",
    "clang-3.3",
    "clang-3.4",
    "c++",
    "g++",
    "g++-4.1",
    "g++-4.2",
    "g++-4.3",
    "g++-4.4",
    "g++-4.5",
    "g++-4.6",
    "g++-4.7",
    "g++-4.8",
    "llvm-g++",
    "clang++",
    0
};

static char const * extensions[] =
{
    ".c",
    ".C",
    ".cc",
    ".cxx",
    ".c++",
    ".C++",
    ".cpp",
    ".cp",
    ".i",
    ".ii",
    ".m",
    ".S",
    0
};

typedef struct bear_command_config_t
{
    char const * output_file;
    char const * libear_file;
    char const * socket_dir;
    char const * socket_file;
    char * const * unprocessed_argv;
    int debug : 1;
    int print_compilers : 1;
    int print_extensions : 1;
} bear_command_config_t;

// variables which are used in signal handler
static volatile pid_t    child_pid;
static volatile int      child_status = EXIT_FAILURE;

// forward declare the used methods
static void mask_all_signals(int command);
static void install_signal_handler(int signum);
static void collect_messages(char const * socket, char const * output, bear_output_filter_t const * cfg, int sync_fd);
static void update_environment(char const * key, char const * value);
static void prepare_socket_file(bear_command_config_t *);
static void teardown_socket_file(bear_command_config_t *);
static void notify_child(int fd);
static void wait_for_parent(int fd);
static void parse(int argc, char * const argv[], bear_command_config_t * config);

static void print_version();
static void print_usage(char const * const name);
static void print_known_compilers(bear_output_filter_t const * filter);
static void print_known_extensions(bear_output_filter_t const * filter);


int main(int argc, char * const argv[])
{
    bear_output_filter_t filter = {
        .compilers = compilers,
        .extensions = extensions
    };
    bear_command_config_t commands = {
        .output_file = DEFAULT_OUTPUT_FILE,
        .libear_file = DEFAULT_PRELOAD_FILE,
        .socket_dir = 0,
        .socket_file = 0,
        .unprocessed_argv = 0,
        .debug = 0,
        .print_compilers = 0,
        .print_extensions = 0
    };
    int sync_fd[2];

    parse(argc, argv, &commands);
    if (commands.print_compilers)
    {
        print_known_compilers(&filter);
        exit(EXIT_SUCCESS);
    }
    if (commands.print_extensions)
    {
        print_known_extensions(&filter);
        exit(EXIT_SUCCESS);
    }
    prepare_socket_file(&commands);
    // set up sync pipe
    if (-1 == pipe(sync_fd))
    {
        perror("bear: pipe");
        exit(EXIT_FAILURE);
    }
    // fork
    child_pid = fork();
    if (-1 == child_pid)
    {
        perror("bear: fork");
        exit(EXIT_FAILURE);
    }
    else if (0 == child_pid)
    {
        // child process
        close(sync_fd[1]);
        wait_for_parent(sync_fd[0]);
        update_environment(ENV_PRELOAD, commands.libear_file);
        update_environment(ENV_OUTPUT, commands.socket_file);
#ifdef ENV_FLAT
        update_environment(ENV_FLAT, "1");
#endif
        if (commands.socket_dir)
        {
            free((void *)commands.socket_dir);
            free((void *)commands.socket_file);
        }
        if (-1 == execvp(*commands.unprocessed_argv, commands.unprocessed_argv))
        {
            perror("bear: execvp");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        // parent process
        install_signal_handler(SIGCHLD);
        install_signal_handler(SIGINT);
        mask_all_signals(SIG_BLOCK);
        close(sync_fd[0]);
        collect_messages(
            commands.socket_file,
            commands.output_file,
            commands.debug ? 0 : &filter,
            sync_fd[1]);
        teardown_socket_file(&commands);
    }
    return child_status;
}

static void collect_messages(char const * socket_file, char const * output_file, bear_output_filter_t const * cfg, int sync_fd)
{
    bear_output_t * handle = bear_open_json_output(output_file, cfg);
    int s = bear_create_unix_socket(socket_file);
    notify_child(sync_fd);
    // receive messages
    bear_message_t msg;
    mask_all_signals(SIG_UNBLOCK);
    while ((child_pid) && bear_accept_message(s, &msg))
    {
        mask_all_signals(SIG_BLOCK);
        bear_append_json_output(handle, &msg);
        bear_free_message(&msg);
        mask_all_signals(SIG_UNBLOCK);
    }
    mask_all_signals(SIG_BLOCK);
    // release resources
    bear_close_json_output(handle);
    close(s);
}

static void update_environment(char const * key, char const * value)
{
    if (-1 == setenv(key, value, 1))
    {
        perror("bear: setenv");
        exit(EXIT_FAILURE);
    }
}

static void prepare_socket_file(bear_command_config_t * config)
{
    // create temporary directory for socket
    if (0 == config->socket_file)
    {
        char template[] = "/tmp/bear-XXXXXX";
        char const * temp_dir = mkdtemp(template);
        if (0 == temp_dir)
        {
            perror("bear: mkdtemp");
            exit(EXIT_FAILURE);
        }
        if (-1 == asprintf((char **)&(config->socket_file), "%s/socket", temp_dir))
        {
            perror("bear: asprintf");
            exit(EXIT_FAILURE);
        }
        config->socket_dir = strdup(temp_dir);
        if (0 == config->socket_dir)
        {
            perror("bear: strdup");
            exit(EXIT_FAILURE);
        }
    }
    // remove old socket file if any
    if ((-1 == unlink(config->socket_file)) && (ENOENT != errno))
    {
        perror("bear: unlink");
        exit(EXIT_FAILURE);
    }
}

static void teardown_socket_file(bear_command_config_t * config)
{
    unlink(config->socket_file);
    if (config->socket_dir)
    {
        rmdir(config->socket_dir);
        free((void *)config->socket_dir);
        free((void *)config->socket_file);
    }
}

static void parse(int argc, char * const argv[], bear_command_config_t * commands)
{
    // parse command line arguments.
    int opt;
    while ((opt = getopt(argc, argv, "o:b:s:dcevh?")) != -1)
    {
        switch (opt)
        {
        case 'o':
            commands->output_file = optarg;
            break;
        case 'b':
            commands->libear_file = optarg;
            break;
        case 's':
            commands->socket_file = optarg;
            break;
        case 'd':
            commands->debug = 1;
            break;
        case 'c':
            commands->print_compilers = 1;
            break;
        case 'e':
            commands->print_extensions = 1;
            break;
        case 'v':
            print_version();
            exit(EXIT_SUCCESS);
        case 'h':
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
        default: /* '?' */
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    // validate
    if (argc == optind)
    {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    commands->unprocessed_argv = &(argv[optind]);
}

static void handler(int signum)
{
    switch (signum)
    {
    case SIGCHLD:
    {
        int status;
        while (0 > waitpid(WAIT_ANY, &status, WNOHANG)) ;
        child_status = WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE;
        child_pid = 0;
        break;
    }
    case SIGINT:
        kill(child_pid, signum);
    default:
        break;
    }
}

static void install_signal_handler(int signum)
{
    struct sigaction action;
    action.sa_handler = handler;
    action.sa_flags = 0;
    if (0 != sigemptyset(&action.sa_mask))
    {
        perror("bear: sigemptyset");
        exit(EXIT_FAILURE);
    }
    if (0 != sigaddset(&action.sa_mask, signum))
    {
        perror("bear: sigaddset");
        exit(EXIT_FAILURE);
    }
    if (0 != sigaction(signum, &action, NULL))
    {
        perror("bear: sigaction");
        exit(EXIT_FAILURE);
    }
}

static void mask_all_signals(int command)
{
    sigset_t signal_mask;
    if (0 != sigfillset(&signal_mask))
    {
        perror("bear: sigfillset");
        exit(EXIT_FAILURE);
    }
    if (0 != sigprocmask(command, &signal_mask, 0))
    {
        perror("bear: sigprocmask");
        exit(EXIT_FAILURE);
    }
}

static void notify_child(int fd)
{
    if (-1 == write(fd, "ready", 5))
    {
        perror("bear: write");
        exit(EXIT_FAILURE);
    }
    close(fd);
}

static void wait_for_parent(int fd)
{
    char buffer[5];
    if (-1 == read(fd, buffer, sizeof(buffer)))
    {
        perror("bear: read");
        exit(EXIT_FAILURE);
    }
    close(fd);
}

static void print_version()
{
    fprintf(stdout,
            "Bear %s\n"
            "Copyright (C) 2012, 2013 by László Nagy\n"
            "This is free software; see the source for copying conditions.  There is NO\n"
            "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n",
            BEAR_VERSION);
}

static void print_usage(char const * const name)
{
    fprintf(stderr,
            "Usage: %s [-o output] [-b libear] [-d socket] -- command\n"
            "\n"
            "   -o output   output file (default: %s)\n"
            "   -b libear   library location (default: %s)\n"
            "   -s socket   multiplexing socket (default: randomly generated)\n"
            "   -d          debug output (default: disabled)\n"
            "   -c          prints known compilers and exit\n"
            "   -e          prints known source file extensions and exit\n"
            "   -v          prints Bear version and exit\n"
            "   -h          this message\n",
            name,
            DEFAULT_OUTPUT_FILE,
            DEFAULT_PRELOAD_FILE);
}

static void print_array(char const * const * const in)
{
    char const * const * it = in;
    for (; *it; ++it)
    {
        printf("  %s\n",*it);
    }
}

static void print_known_compilers(bear_output_filter_t const * filter)
{
    print_array(filter->compilers);
}

static void print_known_extensions(bear_output_filter_t const * filter)
{
    print_array(filter->extensions);
}
