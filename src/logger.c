#include "logger.h"
#include <stdio.h>

static FILE *log_file = NULL;

void logger_init(const char *path)
{
    log_file = fopen(path, "w");

    if (!log_file)
    {
        printf("Cannot open log file: %s\n", path);
        return;
    }

    fprintf(log_file, "timestamp,task,event,priority,response_time,execution_time,deadline,result\n");
}

void logger_close(void)
{
    if (log_file)
    {
        fclose(log_file);
        log_file = NULL;
    }
}

void log_event(
    int timestamp,
    const char *task,
    const char *event,
    int priority,
    int response_time,
    int execution_time,
    int deadline,
    const char *result
)
{
    if (!log_file) return;

    fprintf(
        log_file,
        "%d,%s,%s,%d,%d,%d,%d,%s\n",
        timestamp,
        task,
        event,
        priority,
        response_time,
        execution_time,
        deadline,
        result
    );

    fflush(log_file);
}