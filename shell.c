
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include "parser.h"
#include "jobs.h"

#define DEF_MODE S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH

/*
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;   /* defined in libc */
char prompt[] = "tsh> "; /* command line prompt (DO NOT CHANGE) */
int verbose = 0;         /* if true, print additional output */
char sbuf[MAXLINE];      /* for composing sprintf messages */

/* End global variables */

/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
// void exec_job(Expr *ast, int bg, char *cmdline);
int builtin_cmd(char **argv);
void exec_cmd(Cmd *cmd);
void exec_job(struct job_t *job);

void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv, int *argc);
void sigquit_handler(int sig);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine
 */
int main(int argc, char **argv)
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */
    struct job_t *job;

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF)
    {
        switch (c)
        {
        case 'h': /* print help message */
            usage();
            break;
        case 'v': /* emit additional diagnostic info */
            verbose = 1;
            break;
        case 'p':            /* don't print a prompt */
            emit_prompt = 0; /* handy for automatic testing */
            break;
        default:
            usage();
        }
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT, sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler); /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler); /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler);

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1)
    {

        /* Read command line */
        if (emit_prompt)
        {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
        {
            printf("fgets error: %d", errno);
            exit(0);
        }

        if (feof(stdin))
        { /* End of file (ctrl-d) */
            fflush(stdout);
            exit(0);
        }

        /* Evaluate the command line */
        eval(cmdline);
        fflush(stdout);
        fflush(stdout);

        while (job = pop_from_queue())
        {
            exec_job(job);
        }
    }

    exit(0); /* control never reaches here */
}

/*
 * eval - Evaluate the command line that the user has just typed in
 *
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.
 */

// want to get
void eval(char *cmdline)
{
    int bg, output_fd, input_fd;
    int argc;
    char *argv[MAXARGS];
    int fd;

    // gets pgid of parent
    pid_t pgid = getpgid(0);
    int state;

    bg = parseline(cmdline, argv, &argc);

    if (!argv[0])
    {
        return;
    }
    struct token **tokens = malloc(argc * sizeof(struct token *));
    if (tokenize(argv, argc, tokens) < 0)
    {
        exit(0);
    }

    if (!bg)
    {
        state = FG;
    }
    else
    {
        state = BG;
    }

    // struct job_t *job = getjobjid(jobs, jid);

    // initial pgid is parents
    int jid = addjob(jobs, pgid, state, cmdline);
    struct job_t *job = getjobjid(jobs, jid);
    job->num_tasks = get_pipeline(tokens, argc, job->tasks);
    // printf("%d\n", job->num_tasks);
    // for (int i = 0; i < job->num_tasks; i++)
    // {
    //     // echo "hi" > 3 | cat yo | ho
    //     Cmd *cmd = job->tasks[i];

    //     printf("%s %d\n", cmd->args[0], cmd->argc);
    // }
    // exit(0);

    exec_job(job);

    // exec_cmd(job->tasks[job->curr_task], bg, input_fd, output_fd, jid, cmdline);
}

void exec_job(struct job_t *job)
{
    Cmd *cmd = job->tasks[0];
    int input_fd = job->input_fd;
    int output_fd = job->output_fd;
    int pipefd[2];
    int redirect_to_next = 0;
    int is_first_task;
    pid_t pid = -1;
    sigset_t mask, prev_mask;
    int fd;

    job->running = 1;

    while (pid == -1 || fgpgid(jobs) == job->pgid)
    {
        is_first_task = (job->curr_task == 0);
        // rediredct output to next task in pipeline
        if (job->curr_task < job->num_tasks - 1)
        {
            pipe(pipefd);
            if (job->output_fd != STDOUT_FILENO)
            {
                close(job->output_fd);
            }
            job->output_fd = pipefd[1];
            redirect_to_next = 1;
        }

        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        sigprocmask(SIG_BLOCK, &mask, &prev_mask);

        if ((pid = fork()) == 0)
        {
            sigprocmask(SIG_SETMASK, &prev_mask, NULL);

            if (job->curr_task == 0)
            {
                if (setpgid(0, 0) == -1)
                {
                    unix_error("error setting process group");
                    exit(0);
                }
            }
            else
            {
                setpgid(0, job->pgid);
            }

            dup2(output_fd, STDOUT_FILENO);
            dup2(input_fd, STDIN_FILENO);

            exec_cmd(job->tasks[job->curr_task]);
        }
        else
        {
            add_process_to_job(jobs, pid);
            if (is_first_task)
            {
                // set job process group pgid to first process pid
                job->pgid = pid;
            }
            if (job->state == FG)
            {
                sigprocmask(SIG_SETMASK, &prev_mask, NULL);
                waitfg(pid);
                // sigsuspend(&prev_mask);
            }
            else
            {
                int jid = pid2jid(pid);
                sigprocmask(SIG_SETMASK, &prev_mask, NULL);
                printf("[%d] (%d) %s", jid, pid, job->cmdline);
            }
            sigprocmask(SIG_SETMASK, &prev_mask, NULL);
        }

        if (redirect_to_next)
        {
            if (job->input_fd != STDIN_FILENO)
            {
                close(job->input_fd);
            }
            job->input_fd = pipefd[1];
            redirect_to_next = 0;
        }
    }
}

void exec_cmd(Cmd *cmd)
{
    int fd;
    char **args = cmd->args;
    struct Io_redirect *redirects = cmd->redirects;
    int redirectc = cmd->redirectc;

    if (builtin_cmd(args))
    {
        return;
    }

    for (int i = 0; i < redirectc; i++)
    {

        switch (redirects[i].op)
        {
        case REDIRECT_IN:
            fd = open(redirects[i].file, O_RDWR, DEF_MODE);
            lseek(fd, 0, SEEK_SET);
            dup2(fd, STDIN_FILENO);
            break;
        case REDIRECT_OUT:
            fd = open(redirects[i].file, O_RDWR | O_CREAT | O_TRUNC, DEF_MODE);
            dup2(fd, STDOUT_FILENO);

            break;
        case REDIRECT_ERR:
            fd = open(redirects[i].file, O_RDWR | O_CREAT, DEF_MODE);
            dup2(fd, STDERR_FILENO);
            break;
        case REDIRECT_OUT_APPEND:
            fd = open(redirects[i].file, O_RDWR | O_APPEND | O_CREAT, DEF_MODE);
            dup2(fd, STDOUT_FILENO);
        }
    }

    if (execve(args[0], args, environ) < 0)
    {

        printf("%s: command not found\n", args[0]);
        exit(0);
    };
}

/*
 * parseline - Parse the command line and build the argv array.
 *
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.
 */
int parseline(const char *cmdline, char **argv, int *argc)
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
                                /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf) - 1] = ' ';   /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
        buf++;

    /* Build the argv list */
    *argc = 0;
    if (*buf == '\'')
    {
        buf++;
        delim = strchr(buf, '\'');
    }
    else
    {
        delim = strchr(buf, ' ');
    }

    while (delim)
    {
        argv[(*argc)++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* ignore spaces */
            buf++;

        if (*buf == '\'')
        {
            buf++;
            delim = strchr(buf, '\'');
        }
        else
        {
            delim = strchr(buf, ' ');
        }
    }
    argv[*argc] = NULL;

    if (argc == 0) /* ignore blank line */
        return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[*argc - 1] == '&')) != 0)
    {
        --(*argc);
        argv[*argc] = NULL;
    }
    return bg;
}

/*
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.
 */
int builtin_cmd(char **argv)
{
    if (!strcmp(argv[0], "quit"))
    {
        exit(0);
    }
    else if (!strcmp(argv[0], "jobs"))
    {
        listjobs(jobs);
        return 1;
    }
    else if (!strcmp(argv[0], "bg") || !strcmp(argv[0], "fg"))
    {
        do_bgfg(argv);
        return 1;
    }
    return 0; /* not a builtin command */
}

/*
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv)
{

    struct job_t *job;

    if (argv[1] == NULL)
    {
        printf("%s command requires PID or %%jobid argument \n", argv[0]);
        return;
    }

    if (argv[1][0] == '%')
    {
        int job_id;
        char *job_param = argv[1] + 1;

        if (!(job_id = atoi(job_param)))
        {
            printf("%%(%d): No such job\n", job_id);
            return;
        }

        job = getjobjid(jobs, job_id);
        if (job == NULL)
        {
            printf("%%%d: No such job\n", job_id);
            return;
        }
    }
    else
    {
        int pid;
        if (!(pid = atoi(argv[1])))
        {
            printf("%s: argument must be a PID or %%jobid \n", argv[0]);
            return;
        }
        job = getjobpid(jobs, pid);

        if (job == NULL)
        {
            printf("(%d): No such process \n", pid);
            return;
        }
    }

    if ((!strcmp("bg", argv[0])))
    {
        job->state = BG;
        printf("[%d] (%d) %s", job->jid, job->pgid, job->cmdline);
        if (kill(-(job->pgid), SIGCONT) == -1)
        {
            unix_error("bg sigcont failed");
        };
    }
    else
    {
        job->state = FG;
        if (kill(-(job->pgid), SIGCONT) == -1)
        {
            unix_error("bg sigcont failed");
        };
        waitfg(job->pgid);
    }
    return;
}

/*
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pgid)
{
    // sigset_t mask;
    // sigemptyset(&mask);
    // sigaddset(&mask, SIGCHLD);
    // sigprocmask(SIG_UNBLOCK, &mask, NULL);
    printf("%d", pgid);
    fflush(stdout);
    struct job_t *job = getjobpgid(pgid);
    while (pgid == fgpgid(jobs) || pgid == 0)
    {
        if (!job->running)
        {
        }

        sleep(0.01);
    }

    return;
}

/*****************
 * Signal handlers
 *****************/

/*
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.
 */
void sigchld_handler(int sig)
{
    int status;
    pid_t pid;
    struct job_t *job;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
    {
        job = getjobpid(jobs, pid);

        if (WIFSTOPPED(status))
        {
            printf("Job [%d] (%d) stopped by signal %d\n", pid2jid(pid), pid, WSTOPSIG(status));
            job->state = ST;
            return;
        }
        else if (WIFSIGNALED(status))
        {
            printf("Job [%d] (%d) terminated by signal %d\n", pid2jid(pid), pid, WTERMSIG(status));
            deletejob(jobs, pid);
        }
        else
        {
            // process naturally terminated
            job->curr_task++;

            if (job->curr_task == job->num_tasks)
            {
                deletejob(jobs, job->pgid);
                return;
            }

            job->running = 0;

            // if background job, add next task to queue
            if (job->state == BG)
            {
                add_job_to_queue(jobs, job->pgid);
            }
        }
    }

    return;
}

/*
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.
 */
void sigint_handler(int sig)
{

    pid_t fgjob = fgpgid(jobs);
    if (!fgjob)
    {
        return;
    }
    if (kill(-fgjob, SIGINT) == -1)
    {
        unix_error("kill call failed");
        exit(0);
    };

    // printf("Job [%d] (%d) terminated by signal 2\n", pid2jid(fgjob), fgjob);

    // deletejob(jobs, fgjob);
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.
 */
void sigtstp_handler(int sig)
{

    pid_t fgjob = fgpgid(jobs);

    if (!fgjob)
    {
        return;
    }
    if (kill(-fgjob, SIGSTOP) == -1)
    {
        unix_error("kill call failed");
        exit(0);
    };

    // struct job_t * job = getjobpid(jobs, fgpid(jobs));
    // job->state = ST;

    return;
}

/*********************
 * End signal handlers
 *********************/

/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void)
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler)
{
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
        unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig)
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}
