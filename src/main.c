#include <stdio.h>
#include "simulator.h"
#include "tasks.h"
#include "logger.h"

int main(void)
{
    logger_init("logs/runtime_log.csv");

    simulator_init();
    create_system_tasks();

    simulator_run(1200);

    logger_close();

    printf("Simulation completed.\n");
    printf("Runtime log saved to logs/runtime_log.csv\n");

    return 0;
}