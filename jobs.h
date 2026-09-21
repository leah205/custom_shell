
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

#ifndef JOBS_H
#define JOBS_H

/* Misc manifest constants */
#define MAXLINE 1024   /* max line size */
#define MAXARGS 128    /* max args on a command line */
#define MAXJOBS 16     /* max jobs at any point in time */
#define MAXPROC 8      /* max processes ina job at any point*/
#define MAXJID 1 << 16 /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

struct job_t
{               /* The job struct */
    pid_t pgid; /* job PID */
    pid_t pids[MAXPROC];
    int num_proc;
    Cmd *tasks[MAX_TASKS];
    int curr_task;
    int jid;               /* job ID [1, 2, ...] */
    int state;             /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE]; /* command line */
    int input_fd;
    int output_fd;
    int num_tasks;
    int running;
};
extern struct job_t jobs[MAXJOBS]; /* The job list */

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs);
int addjob(struct job_t *jobs, pid_t pgid, int state, char *cmdline);
int add_job_to_queue(struct job_t *jobs, pid_t pgid);
int add_process_to_job(struct job_t *jobs, pid_t pid);
struct job_t *pop_from_queue();

int deletejob(struct job_t *jobs, pid_t pgid);
pid_t fgpgid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobpgid(struct job_t *jobs, pid_t pgid);
struct job_t *getjobjid(struct job_t *jobs, int jid);
int pid2jid(pid_t pid);

void listjobs(struct job_t *jobs);

#endif