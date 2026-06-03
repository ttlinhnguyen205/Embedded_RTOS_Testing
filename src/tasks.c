#include "tasks.h"
#include "simulator.h"

void create_system_tasks(void)
{
    simulator_add_task("Emergency_Task",        5, 0,   10, 20,  DEADLINE_HARD);
    simulator_add_task("Control_Task",          4, 50,  15, 50,  DEADLINE_HARD);
    simulator_add_task("Deadline_Monitor_Task", 4, 100, 10, 100, DEADLINE_HARD);
    simulator_add_task("Sensor_Task",           3, 100, 10, 100, DEADLINE_SOFT);
    simulator_add_task("Communication_Task",    2, 150, 15, 150, DEADLINE_SOFT);
    simulator_add_task("Fault_Injector_Task",   2, 500, 20, 500, DEADLINE_SOFT);
    simulator_add_task("Logger_Task",           1, 200, 20, 200, DEADLINE_SOFT);
}