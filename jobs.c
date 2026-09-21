
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
/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */

pid_t jobs_queue[MAXJOBS]; /* jids waiting to be scheduled*/

int nextjid = 1;
int jobs_in_queue = 0;
struct job_t jobs[MAXJOBS]; /* The job list */

void clearjob(struct job_t *job)
{
    job->pgid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
    job->running = 0;
    if (!job->input_fd)
    {
        job->input_fd = STDIN_FILENO;
    }
    else if (job->input_fd != STDIN_FILENO)
    {

        close(job->input_fd);
        job->input_fd = STDIN_FILENO;
    }
    if (!job->output_fd)
    {
        job->output_fd = STDOUT_FILENO;
    }
    else if (job->output_fd != STDOUT_FILENO)
    {
        close(job->output_fd);
        job->output_fd = STDOUT_FILENO;
    }

    for (int i = 0; i < MAXPROC; i++)
    {
        job->pids[i] = 0;
    }
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
        clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs)
{
    int i, max = 0;

    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid > max)
            max = jobs[i].jid;
    return max;
}

int add_process_to_job(struct job_t *job, pid_t pid)
{
    if (job->num_proc == MAXPROC)
    {
        return 0;
    }
    job->pids[job->num_proc++] = pid;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pgid, int state, char *cmdline)
{
    int i;

    if (pgid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pgid == 0)
        {
            jobs[i].pgid = pgid;
            jobs[i].state = state;
            jobs[i].jid = nextjid++;
            jobs[i].curr_task = 0;
            jobs[i].input_fd = STDIN_FILENO;
            jobs[i].output_fd = STDOUT_FILENO;
            if (nextjid > MAXJOBS)
                nextjid = 1;
            strcpy(jobs[i].cmdline, cmdline);
            // if (verbose)
            // {
            //     printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pgid, jobs[i].cmdline);
            // }
            return jobs[i].jid;
        }
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

int add_job_to_queue(struct job_t *jobs, pid_t pgid)
{
    if (jobs_in_queue == MAXJOBS)
    {
        return 0;
    }
    jobs_queue[jobs_in_queue++] = pgid;
    return 1;
}

struct job_t *pop_from_queue()
{
    pid_t pgid;
    if (!jobs_in_queue)
    {
        return NULL;
    }

    pgid = jobs_queue[0];
    struct job_t *job = getjobpgid(jobs, pgid);

    for (int i = 0; i < jobs_in_queue - 1; i++)
    {
        jobs_queue[i] = jobs_queue[i + 1];
    }
    jobs_in_queue--;
    return job;
}

struct job_t *remove_from_queue(pid_t pgid)
{
    struct job_t *job = NULL;
    for (int i = 0; i < jobs_in_queue; i++)
    {
        if (jobs_queue[i] == pgid)
        {
            for (int j = i; j < jobs_in_queue - 1; j++)
            {
                jobs_queue[i] = jobs_queue[i + 1];
            }
            break;
        }
    }
    jobs_in_queue--;

    return job;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pgid)
{
    int i;

    if (pgid < 1)
        return 0;

    remove_from_queue(pgid);

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pgid == pgid)
        {
            clearjob(&jobs[i]);
            nextjid = maxjid(jobs) + 1;
            return 1;
        }
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpgid(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].state == FG)
            return jobs[i].pgid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid)
{
    int i;

    if (pid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
    {
        struct job_t *job = &jobs[i];
        for (int j = 0; j < job->num_proc; j++)
        {
            if (job->pids[j] == pid)
            {
                return job;
            }
        }
        // if (jobs[i].pgid == pgid)
        //     return &jobs[i];
    }
    return NULL;
}

struct job_t *getjobpgid(struct job_t *jobs, pid_t pgid)
{
    for (int i = 0; i < MAXJOBS; i++)
    {
        struct job_t *job = &jobs[i];
        if (job->pgid == pgid)
        {
            return job;
        }
    }
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid)
{
    int i;

    if (jid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid == jid)
            return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pgid)
{
    int i;

    if (pgid < 1)
        return 0;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pgid == pgid)
        {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pgid != 0)
        {
            printf("[%d] (%d) ", jobs[i].jid, jobs[i].pgid);
            switch (jobs[i].state)
            {
            case BG:
                printf("Running ");
                break;
            case FG:
                printf("Foreground ");
                break;
            case ST:
                printf("Stopped ");
                break;
            default:
                printf("listjobs: Internal error: job[%d].state=%d ",
                       i, jobs[i].state);
            }
            printf("%s", jobs[i].cmdline);
        }
    }
}
/******************************
 * end job list helper routines
 ******************************/
