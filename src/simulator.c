#include "simulator.h"
#include "logger.h"
#include <stdio.h>
#include <string.h>

static SimTask tasks[MAX_TASKS];
static int task_count = 0;
static int tick = 0;
static int context_switch_count = 0;
static int cpu_busy_ticks = 0;

void simulator_init(void)
{
    memset(tasks, 0, sizeof(tasks));
    task_count = 0;
    tick = 0;
    context_switch_count = 0;
    cpu_busy_ticks = 0;
}

void simulator_add_task(
    const char *name,
    int priority,
    int period_ms,
    int wcet_ms,
    int deadline_ms,
    DeadlineType deadline_type
)
{
    if (task_count >= MAX_TASKS) return;

    SimTask *t = &tasks[task_count++];

    strncpy(t->name, name, MAX_NAME_LEN - 1);
    t->priority = priority;
    t->period_ms = period_ms;
    t->wcet_ms = wcet_ms;
    t->deadline_ms = deadline_ms;
    t->deadline_type = deadline_type;

    t->next_release = period_ms > 0 ? 0 : -1;
    t->remaining_time = 0;
    t->ready_time = -1;
    t->start_time = -1;
    t->finish_time = -1;
    t->response_time = 0;
    t->execution_time = 0;
    t->deadline_miss_count = 0;
    t->active = 0;
}

static void release_periodic_tasks(void)
{
    for (int i = 0; i < task_count; i++)
    {
        SimTask *t = &tasks[i];

        if (t->period_ms <= 0) continue;

        if (tick == t->next_release && !t->active)
        {
            t->active = 1;
            t->remaining_time = t->wcet_ms;
            t->ready_time = tick;
            t->start_time = -1;
            t->next_release += t->period_ms;

            log_event(tick, t->name, "READY", t->priority, 0, 0, t->deadline_ms, "INFO");
        }
    }
}

void simulator_trigger_emergency(void)
{
    for (int i = 0; i < task_count; i++)
    {
        SimTask *t = &tasks[i];

        if (strcmp(t->name, "Emergency_Task") == 0 && !t->active)
        {
            t->active = 1;
            t->remaining_time = t->wcet_ms;
            t->ready_time = tick;
            t->start_time = -1;

            log_event(tick, t->name, "READY", t->priority, 0, 0, t->deadline_ms, "INFO");
        }
    }
}

void simulator_inject_artificial_delay(const char *task_name, int extra_delay)
{
    for (int i = 0; i < task_count; i++)
    {
        if (strcmp(tasks[i].name, task_name) == 0 && tasks[i].active)
        {
            tasks[i].remaining_time += extra_delay;

            log_event(
                tick,
                task_name,
                "ARTIFICIAL_DELAY",
                tasks[i].priority,
                0,
                extra_delay,
                tasks[i].deadline_ms,
                "INFO"
            );
        }
    }
}

static SimTask *pick_highest_priority_task(void)
{
    SimTask *selected = NULL;

    for (int i = 0; i < task_count; i++)
    {
        if (!tasks[i].active) continue;

        if (selected == NULL || tasks[i].priority > selected->priority)
        {
            selected = &tasks[i];
        }
    }

    return selected;
}

static void finish_task(SimTask *t)
{
    t->finish_time = tick;
    t->execution_time = t->finish_time - t->start_time;
    t->response_time = t->start_time - t->ready_time;

    int total_time = t->finish_time - t->ready_time;
    const char *result = "PASS";

    if (total_time > t->deadline_ms)
    {
        t->deadline_miss_count++;
        result = (t->deadline_type == DEADLINE_HARD) ? "FAIL" : "PASS_SOFT_DELAY";

        log_event(
            tick,
            t->name,
            "DEADLINE_MISS",
            t->priority,
            t->response_time,
            t->execution_time,
            t->deadline_ms,
            result
        );
    }
    if (strcmp(t->name, "Logger_Task") == 0)
    {
        log_event(
            tick,
            t->name,
            "MUTEX_UNLOCK_LOG_BUFFER",
            t->priority,
            t->response_time,
            t->execution_time,
            t->deadline_ms,
            "INFO"
        );
    }

    log_event(
        tick,
        t->name,
        "FINISH",
        t->priority,
        t->response_time,
        t->execution_time,
        t->deadline_ms,
        result
    );

    t->active = 0;
    t->remaining_time = 0;
}

void simulator_run(int duration_ms)
{
    SimTask *current = NULL;

    for (tick = 0; tick <= duration_ms; tick++)
    {
        release_periodic_tasks();

        if (tick == 220)
        {
            log_event(tick, "Sensor_Task", "INTRUSION_DETECTED", 3, 0, 0, 0, "INFO");
            simulator_trigger_emergency();
        }

        if (tick == 500)
        {
            log_event(tick, "Sensor_Task", "FIRE_DETECTED", 3, 0, 0, 0, "INFO");
            simulator_trigger_emergency();
        }

        if (tick == 760)
        {
            log_event(tick, "Fault_Injector_Task", "CPU_OVERLOAD", 2, 0, 0, 0, "INFO");
            simulator_inject_artificial_delay("Control_Task", 45);
        }

        SimTask *next = pick_highest_priority_task();

        if (next)
        {
            if (current != next)
            {
                context_switch_count++;

                if (current && current->active)
                {
                    log_event(
                        tick,
                        current->name,
                        "PREEMPTED",
                        current->priority,
                        0,
                        0,
                        current->deadline_ms,
                        "INFO"
                    );
                }

                log_event(
                    tick,
                    next->name,
                    "CONTEXT_SWITCH",
                    next->priority,
                    0,
                    0,
                    next->deadline_ms,
                    "INFO"
                );

                if (next->start_time < 0)
                {
                    next->start_time = tick;

                    log_event(
                        tick,
                        next->name,
                        "START",
                        next->priority,
                        next->start_time - next->ready_time,
                        0,
                        next->deadline_ms,
                        "RUNNING"
                    );
                    if (strcmp(next->name, "Sensor_Task") == 0)
                    {
                        log_event(
                            tick,
                            next->name,
                            "QUEUE_SEND_SENSOR_DATA",
                            next->priority,
                            0,
                            0,
                            next->deadline_ms,
                            "INFO"
                        );
                    }

                    if (strcmp(next->name, "Control_Task") == 0)
                    {
                        log_event(
                            tick,
                            next->name,
                            "QUEUE_RECEIVE_SENSOR_DATA",
                            next->priority,
                            0,
                            0,
                            next->deadline_ms,
                            "INFO"
                        );
                    }

                    if (strcmp(next->name, "Logger_Task") == 0)
                    {
                        log_event(
                            tick,
                            next->name,
                            "MUTEX_LOCK_LOG_BUFFER",
                            next->priority,
                            0,
                            0,
                            next->deadline_ms,
                            "INFO"
                        );

                        log_event(
                            tick,
                            next->name,
                            "LOGGER_WRITE",
                            next->priority,
                            0,
                            0,
                            next->deadline_ms,
                            "INFO"
                        );
                    }
                    
                }

                current = next;
            }

            next->remaining_time--;
            cpu_busy_ticks++;

            if (next->remaining_time <= 0)
            {
                finish_task(next);
                current = NULL;
            }
        }
    }

    log_event(tick, "SYSTEM", "SUMMARY_CONTEXT_SWITCH", 0, 0, context_switch_count, 0, "INFO");
    log_event(tick, "SYSTEM", "SUMMARY_CPU_BUSY_TICKS", 0, 0, cpu_busy_ticks, 0, "INFO");
}