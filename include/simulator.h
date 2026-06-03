#ifndef SIMULATOR_H
#define SIMULATOR_H

#define MAX_TASKS 10
#define MAX_NAME_LEN 64

typedef enum {
    DEADLINE_HARD,
    DEADLINE_SOFT
} DeadlineType;

typedef struct {
    char name[MAX_NAME_LEN];
    int priority;
    int period_ms;
    int wcet_ms;
    int deadline_ms;
    DeadlineType deadline_type;

    int next_release;
    int remaining_time;
    int ready_time;
    int start_time;
    int finish_time;
    int response_time;
    int execution_time;
    int deadline_miss_count;
    int active;
} SimTask;

void simulator_init(void);

void simulator_add_task(
    const char *name,
    int priority,
    int period_ms,
    int wcet_ms,
    int deadline_ms,
    DeadlineType deadline_type
);

void simulator_run(int duration_ms);
void simulator_trigger_emergency(void);
void simulator_inject_artificial_delay(const char *task_name, int extra_delay);

#endif