/*
 * Copyright (c) 2008-2009 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 *
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

/**
 * Note: this file is a little fast-n-loose with OPAL_HAVE_THREADS --
 * it uses this value in run-time "if" conditionals (vs. compile-time
 * #if conditionals).  We also don't protect including <pthread.h>.
 * That's because this component currently only compiles on Linux and
 * Solaris, and both of these OS's have pthreads.  Using the run-time
 * conditionals gives us better compile-time checking, even of code
 * that isn't activated.
 *
 * Note, too, that the functionality in this file does *not* require
 * all the heavyweight OMPI thread infrastructure (e.g., from
 * --enable-mpi-thread-multiple or --enable-progress-threads).  All work that
 * is done in a separate progress thread is very carefully segregated
 * from that of the main thread, and communication back to the main
 * thread
 */

#include "orcm_config.h"

#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <ctype.h>
#include <signal.h>

#include "opal/util/argv.h"
#include "opal/threads/threads.h"

#include "orcm/constants.h"
#include "orcm/mca/notifier/base/base.h"

#include "notifier_command.h"

static void diediedie(int status) __opal_attribute_noreturn__;

/* Structre for holding the argument to stdin_main() */
typedef struct {
    int sat_pipe_fd;
    int sat_severity;
    int sat_errcode;
    char *sat_msg;
} stdin_arg_t;


int orcm_notifier_command_split(const char *cmd_arg, char ***argv_arg)
{
    int i;
    char *cmd, *p, *q, *token_start, **argv = NULL;
    bool in_space, in_quote, in_2quote;

    *argv_arg = NULL;
    cmd = strdup(cmd_arg);
    if (NULL == cmd) {
        return ORCM_ERR_IN_ERRNO;
    }

    in_space = in_quote = in_2quote = false;
    for (token_start = p = cmd; '\0' != *p; ++p) {
        /* If we're in a quoted string, all we're doing it looking for
           the matching end quote.  Note that finding the end quote
           does not necessarily mean the end of the token!  So use the
           normal "I found a space [outside of a quote]" processing to
           find the end of the token. */
        if (in_quote &&
            ('\'' == *p && p > token_start && '\\' != *(p - 1))) {
            in_quote = false;
        } else if (in_2quote && 
                   ('\"' == *p && p > token_start && '\\' != *(p - 1))) {
            in_2quote = false;
        }

        /* If we hit a space, it could be the end of a token -- unless
           we're already in a series of spaces. */
        else if (!in_quote && !in_2quote && isspace(*p)) {
            if (!in_space) {
                /* We weren't in a series of spaces, so this was the
                   end of a token. Save it. */
                in_space = true;
                *p = '\0';
                opal_argv_append_nosize(&argv, token_start);
                token_start = p + 1;
            } else {
                /* We're in a series of spaces, so just move
                   token_start up to the next character. */
                token_start = p + 1;
            }
        } else {
            /* We're not in a series of spaces.  We only need to check
               if we find ' or " to start a quoted string (in which
               case spaces no longer mark the end of a string). */
            in_space = false;
            if ('\'' == *p) {
                in_quote = true;
            } else if ('"' == *p) {
                in_2quote = true;
            }
        }
    }
    if (in_quote || in_2quote) {
        free(cmd);
        opal_argv_free(argv);
        return ORCM_ERR_BAD_PARAM;
    } 

    /* Get the last token, if there is one */
    if (!in_space) {
        opal_argv_append_nosize(&argv, token_start);
    }

    /* Replace escapes and non-escaped quotes */
    for (i = 0; NULL != argv[i]; ++i) {
        for (p = q = argv[i]; '\0' != *p; ++p) {
            if ('\\' == *p) {
                switch (*(p + 1)) {
                    /* For quotes, just copy them over and
                       double-increment p */
                case '\'': *q = *(p + 1); ++p; break;
                case '"': *q = *(p + 1); ++p; break;

                    /* For other normal escapes, insert the right code
                       and double-increment p */
                case 'a': *q = '\a'; ++p; break;
                case 'b': *q = '\b'; ++p; break;
                case 'f': *q = '\f'; ++p; break;
                case 'n': *q = '\n'; ++p; break;
                case 'r': *q = '\r'; ++p; break;
                case 't': *q = '\t'; ++p; break;
                case 'v': *q = '\v'; ++p; break;

                    /* For un-terminated escape, just put in a \.  Do
                       *not* double increment p; it's the end of the
                       string! */
                case '\0': *q = '\\'; break;

                    /* Otherwise, just copy and double increment */
                default: *q = *p; ++p; break;
                }
                ++q;
            } else {
                /* Don't copy un-escaped quotes */
                if ('\'' != *p && '"' != *p) {
                    *q = *p;
                    ++q;
                }
            }
        }
        *q = '\0';
    }

    *argv_arg = argv;
    free(cmd);
    return ORCM_SUCCESS;
}


/*
 * Die nicely
 */
static void diediedie(int status)
{
    /* We don't really have any way to report anything, so just close
       the pipe fd and die */
    close(mca_notifier_command_component.to_child[0]);
    close(mca_notifier_command_component.to_parent[1]);
    _exit(status);
}

/*
 * Main entry point for stdin thread
 */
static void *stdin_main(opal_object_t *obj)
{
    char *data;
    opal_thread_t *t = (opal_thread_t*) obj;
    stdin_arg_t *arg = (stdin_arg_t*) t->t_arg;

    asprintf(&data, "<stdin>\n<notifier severity_int=\"%d\" severity_str=\"%s\" errcode=\"%d\">\n<message>%s</message>\n</notifier>\n</stdin>\n",
             arg->sat_severity,
             orcm_notifier_base_sev2str((orcm_notifier_base_severity_t)arg->sat_severity),
             arg->sat_errcode,
             arg->sat_msg);
    if (NULL != data) {
        orcm_notifier_command_write_fd(arg->sat_pipe_fd,
                                       strlen(data) + 1, data);
        free(data);
        close(arg->sat_pipe_fd);
    }

    return NULL;
}

/*
 * Loop over waiting for a child to die
 */
static int do_wait(pid_t pid, int timeout, int *status, bool *exited)
{
    pid_t pid2;
    time_t t1, t2;

    t2 = t1 = time(NULL);
    *exited = false;
    while (timeout <= 0 || t2 - t1 < timeout) {
        pid2 = waitpid(pid, status, WNOHANG);
        if (pid2 == pid) {
            *exited = true;
            return ORCM_SUCCESS;
        } else if (pid2 < 0 && EINTR != errno) {
            if (ECHILD == errno) {
                *exited = true;
                return ORCM_ERR_NOT_FOUND;
            }

            /* What else can we do? */
            diediedie(10);
        }

        /* Let the child run a bit */
        usleep(100);
        t2 = time(NULL);
    }

    return ORCM_ERROR;
}

/*
 * Fork/exec a command from the parent
 */
static void do_exec(void)
{
    pid_t pid;
    bool exited, killed;
    int sel[3], status;
    int pipe_to_stdin[2];
    char *msg, *p, *cmd, **argv = NULL;
    orcm_notifier_command_component_t *c = &mca_notifier_command_component;
    opal_thread_t stdin_thread;
    stdin_arg_t arg;

    /* First three items on the pipe are: severity, errcode, and
       string length (sel = Severity, Errcode, string Length. */
    if (ORCM_SUCCESS != 
        orcm_notifier_command_read_fd(c->to_child[0], sizeof(sel), sel)) {
        diediedie(1);
    }

    /* Malloc out enough space for the string to read */
    msg = malloc(sel[2] + 1);
    if (NULL == msg) {
        diediedie(2);
    }

    if (ORCM_SUCCESS != 
        orcm_notifier_command_read_fd(c->to_child[0], sel[2] + 1, msg)) {
        diediedie(3);
        /* What else can we do? */
    }

    /* We have all the info.  Now build up the string command to
       exec.  Do the $<foo> replacements. */
    cmd = strdup(c->cmd);
    if ('\0' != *cmd) {
        char *temp;

        while (NULL != (p = strstr(cmd, "$s"))) {
            *p = '\0';
            asprintf(&temp, "%s%d%s", cmd, sel[0], p + 2);
            free(cmd);
            cmd = temp;
        }

        while (NULL != (p = strstr(cmd, "$S"))) {
            *p = '\0';
            asprintf(&temp, "%s%s%s", cmd, 
                     orcm_notifier_base_sev2str((orcm_notifier_base_severity_t)sel[0]), p + 2);
            free(cmd);
            cmd = temp;
        }

        while (NULL != (p = strstr(cmd, "$e"))) {
            *p = '\0';
            asprintf(&temp, "%s%d%s", cmd, sel[1], p + 2);
            free(cmd);
            cmd = temp;
        }

        while (NULL != (p = strstr(cmd, "$m"))) {
            *p = '\0';
            asprintf(&temp, "%s%s%s", cmd, msg, p + 2);
            free(cmd);
            cmd = temp;
        }
    }

    /* Now break it up into a list of argv */
    if (ORCM_SUCCESS != orcm_notifier_command_split(cmd, &argv)) {
        diediedie(7);
        /* What else can we do? */
    }

    /* Do we need a stdin pipe? */
    if (mca_notifier_command_component.pass_via_stdin) {
        if (0 != pipe(pipe_to_stdin)) {
            diediedie(8);
        }
    }

    /* Fork off the child and run the command */
    pid = fork();
    if (pid < 0) {
        diediedie(8);
    } else if (pid == 0) {
        int i;
        int fdmax = sysconf(_SC_OPEN_MAX);
        close(0);
        for (i = 3; i < fdmax; ++i) {
            if (!mca_notifier_command_component.pass_via_stdin ||
                pipe_to_stdin[0] != i) {
                close(i);
            }
        }

        /* If we have a pipe to stdin, dup it */
        if (mca_notifier_command_component.pass_via_stdin) {
            close(pipe_to_stdin[1]);
            if (0 != pipe_to_stdin[0]) {
                if (dup2(pipe_to_stdin[0], 0) < 0) {
                    diediedie(13);
                }
                close(pipe_to_stdin[0]);
            }
        }

        /* Run it! */
        execvp(argv[0], argv);
        /* If we get here, bad */
        diediedie(9);
    }

    /* Write down stdin.  Start a thread because this has to run in
       parallel to the timer to kill the grandchild if it runs too
       long. */
    if (mca_notifier_command_component.pass_via_stdin) {
        close(pipe_to_stdin[0]);
        OBJ_CONSTRUCT(&stdin_thread, opal_thread_t);
        stdin_thread.t_run = stdin_main;
        arg.sat_pipe_fd = pipe_to_stdin[1];
        arg.sat_severity = sel[0];
        arg.sat_errcode = sel[1];
        arg.sat_msg = msg;
        stdin_thread.t_arg = (void *) &arg;
        if (OPAL_SUCCESS != opal_thread_start(&stdin_thread)) {
            diediedie(9);
        }
    }

    /* Parent: wait for / reap the child. */
    do_wait(pid, mca_notifier_command_component.timeout, &status, &exited);

    /* If the child didn't die, try killing it nicely.  If that fails, kill
       it dead. */
    killed = false;
    if (!exited) {
        killed = true;
        kill(pid, SIGTERM);
        do_wait(pid, mca_notifier_command_component.timeout, &status, &exited);
        if (!exited) {
            kill(pid, SIGKILL);
            do_wait(pid, mca_notifier_command_component.timeout, &status, 
                    &exited);
        }
    }

    /* Wait for the thread to complete */
    if (mca_notifier_command_component.pass_via_stdin) {
        void *ret;

        close(pipe_to_stdin[1]);
        opal_thread_join(&stdin_thread, &ret);
        OBJ_DESTRUCT(&stdin_thread);
    }

    /* Free stuff */
    free(cmd);
    free(msg);
    opal_argv_free(argv);

    /* Handshake back up to the parent: just send the status value
       back up to the parent and let all interpretation occur up
       there. */
    sel[0] = (int) exited;
    sel[1] = (int) killed;
    sel[2] = status;
    if (ORCM_SUCCESS != 
        orcm_notifier_command_write_fd(mca_notifier_command_component.to_parent[1], 
                                     sizeof(sel), sel)) {
        diediedie(11);
    }
}

/*
 * Main entry point for child
 */
void orcm_notifier_command_child_main(void)
{
    orcm_notifier_command_pipe_cmd_t cmd;
    orcm_notifier_command_component_t *c = &mca_notifier_command_component;

    while (1) {
        /* Block waiting for a command */
        cmd = CMD_MAX;
        if (ORCM_SUCCESS != 
            orcm_notifier_command_read_fd(c->to_child[0], sizeof(cmd), &cmd)) {
            diediedie(4);
        }

        switch (cmd) {
        case CMD_EXEC:
            do_exec();
            break;

        case CMD_TIME_TO_QUIT:
            diediedie(0);

        default:
            diediedie(cmd + 50);
        }
    }
}
