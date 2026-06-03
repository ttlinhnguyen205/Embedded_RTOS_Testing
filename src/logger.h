#ifndef LOGGER_H
#define LOGGER_H

void logger_init(const char *path);
void logger_close(void);

void log_event(
    int timestamp,
    const char *task,
    const char *event,
    int priority,
    int response_time,
    int execution_time,
    int deadline,
    const char *result
);

#endif