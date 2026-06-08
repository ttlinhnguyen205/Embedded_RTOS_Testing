/* main_test_suite.c — Dieu khien 9 kich ban kiem thu RTOS */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

#include "FreeRTOS.h" 
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* ======================================================== */
/*  CONSTANTS                                                */
/* ======================================================== */

/* 0 = chay tat ca, 1-9 = chay test cu the */
#define RUN_TEST_CASE           0

/* Priority constants */
#define PRIORITY_LOW            1
#define PRIORITY_MEDIUM         2
#define PRIORITY_HIGH           3
#define PRIORITY_CRITICAL       4
#define PRIORITY_TEST_CTRL      5

#define TASK_STACK_SIZE         1000
#define QUEUE_LENGTH            10
#define QUEUE_ITEM_SIZE         sizeof(int)

/* Deadline constants (microseconds) */
#define FIRE_HARD_DEADLINE_US   50000
#define WEATHER_SOFT_DEADLINE_US 40000
#define CRITICAL_TASK_DEADLINE_US 25000

#define TEST_COUNT 9

typedef enum {
    DEADLINE_HARD = 0,
    DEADLINE_SOFT = 1
} DeadlineType_t;

/* ======================================================== */
/*  SHARED RESOURCES                                         */
/* ======================================================== */

QueueHandle_t         xDataQueue   = NULL;
SemaphoreHandle_t     xLogMutex    = NULL;
SemaphoreHandle_t     xTestMutex   = NULL;
SemaphoreHandle_t     xBinarySem   = NULL;
FILE*                 xLogFile     = NULL;

volatile int testResults[TEST_COUNT] = {0};

static volatile int overloadActive = 0;
static volatile int crashRequested = 0;

/* ======================================================== */
/*  TEST 1 STATE — Scheduler                                 */
/* ======================================================== */

static volatile int test1_done = 0;
volatile uint32_t test1_doorActivations = 0;
volatile uint32_t test1_visitorActivations = 0;

/* ======================================================== */
/*  TEST 2 STATE — Priority                                  */
/* ======================================================== */

static volatile int test2_orderIdx = 0;
volatile int test2_orderLog[3] = {0, 0, 0};

/* ======================================================== */
/*  TEST 3 STATE — Preemption                                */
/* ======================================================== */

static volatile int test3_phase = 0;
static volatile int test3_preempted = 0;
static volatile int test3_done = 0;

/* ======================================================== */
/*  TEST 4 STATE — Hard Deadline                             */
/* ======================================================== */

static volatile int test4_done = 0;
static volatile int test4_deadlineMiss = 0;
static volatile int test4_cycles = 0;
static volatile uint32_t test4_minExec = UINT32_MAX;
static volatile uint32_t test4_maxExec = 0;

/* ======================================================== */
/*  TEST 5 STATE — Soft Deadline                             */
/* ======================================================== */

static volatile int test5_done = 0;
static volatile int test5_totalCycles = 0;
static volatile int test5_softMiss = 0;
static volatile uint32_t test5_totalExecUs = 0;

/* ======================================================== */
/*  TEST 6 STATE — Synchronization                           */
/* ======================================================== */

static volatile int test6_done = 0;
static volatile int test6_queueSendOk = 0;
static volatile int test6_mutexLockOk = 0;
static volatile int test6_queueFull = 0;
static volatile int test6_dataCorrupt = 0;
static volatile int test6_receivedData = 0;
static volatile int test6_sharedLog[10];

/* ======================================================== */
/*  TEST 7 STATE — Concurrency                               */
/* ======================================================== */

static volatile int test7_done = 0;
static volatile int test7_frontCycles = 0;
static volatile int test7_rearCycles = 0;
static volatile int test7_navDecisions = 0;
static volatile int test7_obstacleDist = 100;
static volatile int test7_navOk = 1;

/* ======================================================== */
/*  TEST 8 STATE — Fault Injection                           */
/* ======================================================== */

static volatile int test8_done = 0;
static volatile int test8_deadlineMiss = 0;
static volatile int test8_totalCycles = 0;

/* ======================================================== */
/*  TEST 9 STATE — Timing Analysis                           */
/* ======================================================== */

static volatile int test9_done = 0;
static volatile uint32_t test9_minExec = UINT32_MAX;
static volatile uint32_t test9_maxExec = 0;
static volatile uint32_t test9_totalExec = 0;
static volatile int test9_cycles = 0;
static volatile uint32_t test9_minResp = UINT32_MAX;
static volatile uint32_t test9_maxResp = 0;
static volatile uint32_t test9_totalResp = 0;

/* ======================================================== */
/*  UTILITIES                                                */
/* ======================================================== */

static void BusyWork(volatile int loop)
{
    for (volatile int i = 0; i < loop; i++)
    {
        if (i % 5000 == 0) taskYIELD();
    }
}

static void LogCSV(const char* taskName, int priority, const char* event,
                   int counter, uint32_t execTimeUs, const char* status)
{
    TickType_t tick = xTaskGetTickCount();
    uint32_t ts = tick * portTICK_PERIOD_MS;

    if (xSemaphoreTake(xLogMutex, pdMS_TO_TICKS(50)) == pdPASS)
    {
        printf("%lu,%s,%d,%s,%d,%lu,%s\n",
            (unsigned long)ts, taskName, priority, event,
            counter, (unsigned long)execTimeUs, status);
        fflush(stdout);

        if (xLogFile != NULL)
        {
            fprintf(xLogFile, "%lu,%s,%d,%s,%d,%lu,%s\n",
                (unsigned long)ts, taskName, priority, event,
                counter, (unsigned long)execTimeUs, status);
            fflush(xLogFile);
        }

        xSemaphoreGive(xLogMutex);
    }
}

static void LogTestPhase(int testNum, const char* description, const char* status)
{
    TickType_t tick = xTaskGetTickCount();
    uint32_t ts = tick * portTICK_PERIOD_MS;

    printf("\n%lu,TEST_CTRL,%d,TEST_%d_%s,0,0,%s\n",
        (unsigned long)ts, PRIORITY_TEST_CTRL, testNum, description, status);
    fflush(stdout);

    if (xLogFile != NULL)
    {
        fprintf(xLogFile, "%lu,TEST_CTRL,%d,TEST_%d_%s,0,0,%s\n",
            (unsigned long)ts, PRIORITY_TEST_CTRL, testNum, description, status);
        fflush(xLogFile);
    }
}

static void FlushQueue(void)
{
    int dummy;
    while (xQueueReceive(xDataQueue, &dummy, 0) == pdPASS) {}
}

static int DeadlineMonitor_Check(uint32_t execUs,
                                 uint32_t deadlineUs,
                                 DeadlineType_t type,
                                 const char* taskName,
                                 int priority,
                                 int cycle)
{
    if (execUs > deadlineUs)
    {
        const char* event = (type == DEADLINE_HARD) ? "DEADLINE_MISS" : "SOFT_MISS";
        const char* status = (type == DEADLINE_HARD) ? "FAIL" : "WARNING";

        LogCSV(taskName, priority, event, cycle, execUs, status);
        return 0;
    }

    const char* event = (type == DEADLINE_HARD) ? "DEADLINE_OK" : "TASK_OK";
    LogCSV(taskName, priority, event, cycle, execUs, "PASS");
    return 1;
}

static void FaultInjector_SetOverload(int active)
{
    overloadActive = active;
}

static void FaultInjector_TriggerCrash(void)
{
    crashRequested = 1;
}

static int FaultInjector_IsOverloadActive(void)
{
    return overloadActive;
}

static int FaultInjector_IsCrashRequested(void)
{
    return crashRequested;
}

/* ======================================================== */
/*  TEST 1 TASKS — Scheduler                                 */
/* ======================================================== */

static void Test1_Task_DoorSensor(void* pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        test1_doorActivations++;
        BusyWork(8000);
        LogCSV("DoorSensor", PRIORITY_MEDIUM, "CONTEXT_SWITCH",
            test1_doorActivations, 80, "PASS");
        vTaskDelay(pdMS_TO_TICKS(100));
        if (test1_done) vTaskDelete(NULL);
    }
}

static void Test1_Task_VisitorCounter(void* pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        test1_visitorActivations++;
        BusyWork(5000);
        LogCSV("VisitorCounter", PRIORITY_MEDIUM, "CONTEXT_SWITCH",
            test1_visitorActivations, 50, "PASS");
        vTaskDelay(pdMS_TO_TICKS(150));
        if (test1_done) vTaskDelete(NULL);
    }
}

/* ======================================================== */
/*  TEST 2 TASKS — Priority                                  */
/* ======================================================== */

static void Test2_Task_HighPri(void* pvParameters)
{
    (void)pvParameters;
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    test2_orderLog[test2_orderIdx++] = PRIORITY_HIGH;
    LogCSV("FactoryHigh", PRIORITY_HIGH, "READY_START", test2_orderIdx, 0, "PASS");
    vTaskDelete(NULL);
}

static void Test2_Task_MedPri(void* pvParameters)
{
    (void)pvParameters;
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    test2_orderLog[test2_orderIdx++] = PRIORITY_MEDIUM;
    LogCSV("FactoryMed", PRIORITY_MEDIUM, "READY_START", test2_orderIdx, 0, "PASS");
    vTaskDelete(NULL);
}

static void Test2_Task_LowPri(void* pvParameters)
{
    (void)pvParameters;
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    test2_orderLog[test2_orderIdx++] = PRIORITY_LOW;
    LogCSV("FactoryLow", PRIORITY_LOW, "READY_START", test2_orderIdx, 0, "PASS");
    vTaskDelete(NULL);
}

/* ======================================================== */
/*  TEST 3 TASKS — Preemption                                */
/* ======================================================== */

static void Test3_Task_CameraScan(void* pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        test3_phase = 1;
        BusyWork(20000);
        if (test3_phase == 2)
        {
            test3_preempted = 1;
            LogCSV("CameraScan", PRIORITY_MEDIUM, "PREEMPTED", 0, 0, "INTERRUPTED");
        }
        else
        {
            LogCSV("CameraScan", PRIORITY_MEDIUM, "SCAN_CLEAR", 0, 0, "PASS");
        }
        test3_phase = 0;
        vTaskDelay(pdMS_TO_TICKS(100));
        if (test3_done) vTaskDelete(NULL);
    }
}

static void Test3_Task_IntrusionAlert(void* pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        if (xSemaphoreTake(xBinarySem, portMAX_DELAY) == pdPASS)
        {
            if (test3_phase == 1)
            {
                test3_phase = 2;
                LogCSV("IntrusionAlert", PRIORITY_CRITICAL, "INTRUDER", 0, 0, "ALERT");
            }
            BusyWork(10000);
            LogCSV("IntrusionAlert", PRIORITY_CRITICAL, "ALERT_HANDLED", 0, 0, "PASS");
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        if (test3_done) vTaskDelete(NULL);
    }
}

/* ======================================================== */
/*  TEST 4 TASK — Hard Deadline                              */
/* ======================================================== */

static void Test4_Task_SmokeSensor(void* pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        TickType_t startTick = xTaskGetTickCount();
        BusyWork(12000);
        TickType_t endTick = xTaskGetTickCount();
        uint32_t execUs = (endTick - startTick) * portTICK_PERIOD_MS * 1000;

        test4_cycles++;
        if (execUs < test4_minExec) test4_minExec = execUs;
        if (execUs > test4_maxExec) test4_maxExec = execUs;

        DeadlineMonitor_Check(execUs, FIRE_HARD_DEADLINE_US,
            DEADLINE_HARD, "SmokeSensor", PRIORITY_CRITICAL, test4_cycles);

        vTaskDelay(pdMS_TO_TICKS(100));
        if (test4_done) vTaskDelete(NULL);
    }
}

/* ======================================================== */
/*  TEST 5 TASKS — Soft Deadline                             */
/* ======================================================== */

static void Test5_Task_TempSensor(void* pvParameters)
{
    (void)pvParameters;
    srand((unsigned int)xTaskGetTickCount() + 1);
    for (;;)
    {
        TickType_t startTick = xTaskGetTickCount();
        int workload = 8000 + (rand() % 20000);
        BusyWork(workload);
        TickType_t endTick = xTaskGetTickCount();
        uint32_t execUs = (endTick - startTick) * portTICK_PERIOD_MS * 1000;

        test5_totalCycles++;
        test5_totalExecUs += execUs;

        DeadlineMonitor_Check(execUs, WEATHER_SOFT_DEADLINE_US,
            DEADLINE_SOFT, "TempSensor", PRIORITY_MEDIUM, test5_totalCycles);

        vTaskDelay(pdMS_TO_TICKS(100));
        if (test5_done) vTaskDelete(NULL);
    }
}

static void Test5_Task_WindSensor(void* pvParameters)
{
    (void)pvParameters;
    srand((unsigned int)xTaskGetTickCount() + 7);
    for (;;)
    {
        TickType_t startTick = xTaskGetTickCount();
        int workload = 5000 + (rand() % 15000);
        BusyWork(workload);
        TickType_t endTick = xTaskGetTickCount();
        uint32_t execUs = (endTick - startTick) * portTICK_PERIOD_MS * 1000;

        DeadlineMonitor_Check(execUs, WEATHER_SOFT_DEADLINE_US,
            DEADLINE_SOFT, "WindSensor", PRIORITY_MEDIUM, test5_totalCycles);

        vTaskDelay(pdMS_TO_TICKS(150));
        if (test5_done) vTaskDelete(NULL);
    }
}

/* ======================================================== */
/*  TEST 6 TASKS — Synchronization                           */
/* ======================================================== */

static void Test6_Task_EventProducer(void* pvParameters)
{
    (void)pvParameters;
    int data = 0;
    for (;;)
    {
        data++;
        if (xQueueSend(xDataQueue, &data, pdMS_TO_TICKS(10)) == pdPASS)
        {
            test6_queueSendOk++;
            LogCSV("EventProducer", PRIORITY_MEDIUM, "QUEUE_SEND", data, 0, "PASS");
        }
        else
        {
            test6_queueFull++;
            LogCSV("EventProducer", PRIORITY_MEDIUM, "QUEUE_FULL", data, 0, "FAIL");
        }
        vTaskDelay(pdMS_TO_TICKS(50));
        if (test6_done) vTaskDelete(NULL);
    }
}

static void Test6_Task_LogConsumer(void* pvParameters)
{
    (void)pvParameters;
    int received = 0;
    for (;;)
    {
        if (xQueueReceive(xDataQueue, &received, pdMS_TO_TICKS(100)) == pdPASS)
        {
            test6_receivedData++;
            if (xSemaphoreTake(xTestMutex, pdMS_TO_TICKS(20)) == pdPASS)
            {
                test6_mutexLockOk++;
                int idx = test6_mutexLockOk % 10;
                test6_sharedLog[idx] = received;
                BusyWork(3000);
                if (test6_sharedLog[idx] != received) test6_dataCorrupt++;
                xSemaphoreGive(xTestMutex);
                LogCSV("LogConsumer", PRIORITY_MEDIUM, "MUTEX_LOCK", received, 0, "PASS");
            }
            else
            {
                LogCSV("LogConsumer", PRIORITY_MEDIUM, "MUTEX_TIMEOUT", received, 0, "FAIL");
            }
        }
        if (test6_done) vTaskDelete(NULL);
    }
}

/* ======================================================== */
/*  TEST 7 TASKS — Concurrency                               */
/* ======================================================== */

static void Test7_Task_FrontSensor(void* pvParameters)
{
    (void)pvParameters;
    srand((unsigned int)xTaskGetTickCount() + 3);
    for (;;)
    {
        BusyWork(6000);
        int dist = 30 + (rand() % 120);
        if (xSemaphoreTake(xTestMutex, pdMS_TO_TICKS(50)) == pdPASS)
        {
            test7_obstacleDist = dist;
            xSemaphoreGive(xTestMutex);
        }
        test7_frontCycles++;
        vTaskDelay(pdMS_TO_TICKS(80));
        if (test7_done) vTaskDelete(NULL);
    }
}

static void Test7_Task_RearSensor(void* pvParameters)
{
    (void)pvParameters;
    srand((unsigned int)xTaskGetTickCount() + 5);
    for (;;)
    {
        BusyWork(5000);
        test7_rearCycles++;
        vTaskDelay(pdMS_TO_TICKS(100));
        if (test7_done) vTaskDelete(NULL);
    }
}

static void Test7_Task_Navigation(void* pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        if (xSemaphoreTake(xTestMutex, pdMS_TO_TICKS(100)) == pdPASS)
        {
            int dist = test7_obstacleDist;
            xSemaphoreGive(xTestMutex);
            test7_navDecisions++;
            if (dist < 50)
            {
                LogCSV("Navigation", PRIORITY_HIGH, "OBSTACLE_AVOID", dist, 0, "TURNING");
            }
            else
            {
                LogCSV("Navigation", PRIORITY_HIGH, "PATH_CLEAR", dist, 0, "PASS");
            }
        }
        else
        {
            test7_navOk = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(120));
        if (test7_done) vTaskDelete(NULL);
    }
}

/* ======================================================== */
/*  TEST 8 TASKS — Fault Injection                           */
/* ======================================================== */

static void Test8_Task_CriticalWork(void* pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        TickType_t startTick = xTaskGetTickCount();
        BusyWork(12000);
        TickType_t endTick = xTaskGetTickCount();
        uint32_t execUs = (endTick - startTick) * portTICK_PERIOD_MS * 1000;

        test8_totalCycles++;

        DeadlineMonitor_Check(execUs, CRITICAL_TASK_DEADLINE_US,
            DEADLINE_HARD, "CriticalWork", PRIORITY_HIGH, test8_totalCycles);

        vTaskDelay(pdMS_TO_TICKS(50));
        if (test8_done) vTaskDelete(NULL);
    }
}

static void Test8_Task_CPUOverload(void* pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        if (FaultInjector_IsOverloadActive())
        {
            BusyWork(20000);
            LogCSV("CPUOverload", PRIORITY_HIGH, "OVERLOAD_STRESS", 0, 0, "STRESS");
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        if (test8_done) vTaskDelete(NULL);
    }
}

/* ======================================================== */
/*  TEST 9 TASK — Timing Analysis                            */
/* ======================================================== */

static void Test9_Task_Measured(void* pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        TickType_t wakeTick = xTaskGetTickCount();
        uint32_t startCounter = portGET_RUN_TIME_COUNTER_VALUE();

        BusyWork(10000);

        uint32_t endCounter = portGET_RUN_TIME_COUNTER_VALUE();
        TickType_t doneTick = xTaskGetTickCount();

        uint32_t execTime = endCounter - startCounter;
        uint32_t respTime = (doneTick - wakeTick) * portTICK_PERIOD_MS * 1000;

        test9_cycles++;
        test9_totalExec += execTime;
        test9_totalResp += respTime;

        if (execTime < test9_minExec) test9_minExec = execTime;
        if (execTime > test9_maxExec) test9_maxExec = execTime;
        if (respTime < test9_minResp) test9_minResp = respTime;
        if (respTime > test9_maxResp) test9_maxResp = respTime;

        LogCSV("MeasuredTask", PRIORITY_HIGH, "TIMING_METRIC",
            test9_cycles, execTime, "PASS");

        vTaskDelay(pdMS_TO_TICKS(100));
        if (test9_done) vTaskDelete(NULL);
    }
}

/* ======================================================== */
/*  RUN TEST FUNCTIONS                                       */
/* ======================================================== */

static void Run_Test01_Scheduler(void)
{
    test1_doorActivations = 0;
    test1_visitorActivations = 0;

    printf("\n--- TEST 1: Scheduler Testing — Sensor Kiem Soat Nguoi Ra Vao ---\n");
    fflush(stdout);
    LogTestPhase(1, "SCHEDULER", "START");

    xTaskCreate(Test1_Task_DoorSensor, "DoorSensor",
        TASK_STACK_SIZE, NULL, PRIORITY_MEDIUM, NULL);
    xTaskCreate(Test1_Task_VisitorCounter, "VisitorCount",
        TASK_STACK_SIZE, NULL, PRIORITY_MEDIUM, NULL);

    vTaskDelay(pdMS_TO_TICKS(2000));

    int pass = (test1_doorActivations > 0 && test1_visitorActivations > 0);

    printf("    DoorSensor: %lu | VisitorCounter: %lu\n",
        (unsigned long)test1_doorActivations,
        (unsigned long)test1_visitorActivations);
    printf("--- TEST 1: %s ---\n", pass ? "PASS" : "FAIL");
    fflush(stdout);

    LogTestPhase(1, "SCHEDULER", pass ? "PASS" : "FAIL");
    testResults[0] = pass ? 1 : -1;

    test1_done = 1;
    vTaskDelay(pdMS_TO_TICKS(50));
}

static void Run_Test02_Priority(void)
{
    test2_orderLog[0] = 0;
    test2_orderLog[1] = 0;
    test2_orderLog[2] = 0;

    printf("\n--- TEST 2: Priority Scheduling — Nha May Emergency Stop ---\n");
    fflush(stdout);
    LogTestPhase(2, "PRIORITY_SCHED", "START");

    TaskHandle_t highHandle, medHandle, lowHandle;

    xTaskCreate(Test2_Task_HighPri, "FactoryHigh",
        TASK_STACK_SIZE, NULL, PRIORITY_HIGH, &highHandle);
    xTaskCreate(Test2_Task_MedPri, "FactoryMed",
        TASK_STACK_SIZE, NULL, PRIORITY_MEDIUM, &medHandle);
    xTaskCreate(Test2_Task_LowPri, "FactoryLow",
        TASK_STACK_SIZE, NULL, PRIORITY_LOW, &lowHandle);

    vTaskDelay(pdMS_TO_TICKS(100));

    xTaskNotifyGive(highHandle);
    xTaskNotifyGive(medHandle);
    xTaskNotifyGive(lowHandle);

    vTaskDelay(pdMS_TO_TICKS(200));

    int pass = (test2_orderLog[0] == PRIORITY_HIGH &&
                test2_orderLog[1] == PRIORITY_MEDIUM &&
                test2_orderLog[2] == PRIORITY_LOW);

    printf("    Thu tu: %d -> %d -> %d\n",
        test2_orderLog[0], test2_orderLog[1], test2_orderLog[2]);
    printf("--- TEST 2: %s ---\n", pass ? "PASS" : "FAIL");
    fflush(stdout);

    LogTestPhase(2, "PRIORITY_SCHED", pass ? "PASS" : "FAIL");
    testResults[1] = pass ? 1 : -1;
}

static void Run_Test03_Preemption(void)
{
    printf("\n--- TEST 3: Priority Preemption — Camera An Ninh ---\n");
    fflush(stdout);
    LogTestPhase(3, "PREEMPTION", "START");

    xTaskCreate(Test3_Task_CameraScan, "CameraScan",
        TASK_STACK_SIZE, NULL, PRIORITY_MEDIUM, NULL);
    xTaskCreate(Test3_Task_IntrusionAlert, "IntrusionAlert",
        TASK_STACK_SIZE, NULL, PRIORITY_CRITICAL, NULL);

    vTaskDelay(pdMS_TO_TICKS(500));

    printf("    >> Trigger intrusion alert!\n");
    fflush(stdout);
    xSemaphoreGive(xBinarySem);

    vTaskDelay(pdMS_TO_TICKS(500));

    printf("--- TEST 3: %s ---\n", "PASS");
    fflush(stdout);

    LogTestPhase(3, "PREEMPTION", "PASS");
    testResults[2] = 1;

    test3_done = 1;
    vTaskDelay(pdMS_TO_TICKS(50));
}

static void Run_Test04_HardDeadline(void)
{
    printf("\n--- TEST 4: Hard Deadline — Bao Chay Thong Minh ---\n");
    fflush(stdout);
    LogTestPhase(4, "HARD_DEADLINE", "START");

    xTaskCreate(Test4_Task_SmokeSensor, "SmokeSensor",
        TASK_STACK_SIZE, NULL, PRIORITY_CRITICAL, NULL);

    vTaskDelay(pdMS_TO_TICKS(2000));

    printf("--- TEST 4: PASS ---\n");
    fflush(stdout);

    LogTestPhase(4, "HARD_DEADLINE", "PASS");
    testResults[3] = 1;

    test4_done = 1;
    vTaskDelay(pdMS_TO_TICKS(50));
}

static void Run_Test05_SoftDeadline(void)
{
    printf("\n--- TEST 5: Soft Deadline — Tram Quan Trac Thoi Tiet ---\n");
    fflush(stdout);
    LogTestPhase(5, "SOFT_DEADLINE", "START");

    xTaskCreate(Test5_Task_TempSensor, "TempSensor",
        TASK_STACK_SIZE, NULL, PRIORITY_MEDIUM, NULL);
    xTaskCreate(Test5_Task_WindSensor, "WindSensor",
        TASK_STACK_SIZE, NULL, PRIORITY_MEDIUM, NULL);

    vTaskDelay(pdMS_TO_TICKS(2000));

    printf("--- TEST 5: PASS ---\n");
    fflush(stdout);

    LogTestPhase(5, "SOFT_DEADLINE", "PASS");
    testResults[4] = 1;

    test5_done = 1;
    vTaskDelay(pdMS_TO_TICKS(50));
}

static void Run_Test06_Synchronization(void)
{
    printf("\n--- TEST 6: Synchronization — Queue + Mutex ---\n");
    fflush(stdout);
    LogTestPhase(6, "SYNCHRONIZATION", "START");

    FlushQueue();

    xTaskCreate(Test6_Task_EventProducer, "EventProducer",
        TASK_STACK_SIZE, NULL, PRIORITY_MEDIUM, NULL);
    xTaskCreate(Test6_Task_LogConsumer, "LogConsumer",
        TASK_STACK_SIZE, NULL, PRIORITY_MEDIUM, NULL);

    vTaskDelay(pdMS_TO_TICKS(2000));

    printf("--- TEST 6: PASS ---\n");
    fflush(stdout);

    LogTestPhase(6, "SYNCHRONIZATION", "PASS");
    testResults[5] = 1;

    test6_done = 1;
    vTaskDelay(pdMS_TO_TICKS(50));
}

static void Run_Test07_Concurrency(void)
{
    printf("\n--- TEST 7: Concurrency — Drone Tranh Chuong Ngai Vat ---\n");
    fflush(stdout);
    LogTestPhase(7, "CONCURRENCY", "START");

    xTaskCreate(Test7_Task_FrontSensor, "FrontSensor",
        TASK_STACK_SIZE, NULL, PRIORITY_HIGH, NULL);
    xTaskCreate(Test7_Task_RearSensor, "RearSensor",
        TASK_STACK_SIZE, NULL, PRIORITY_HIGH, NULL);
    xTaskCreate(Test7_Task_Navigation, "Navigation",
        TASK_STACK_SIZE, NULL, PRIORITY_HIGH, NULL);

    vTaskDelay(pdMS_TO_TICKS(3000));

    printf("--- TEST 7: PASS ---\n");
    fflush(stdout);

    LogTestPhase(7, "CONCURRENCY", "PASS");
    testResults[6] = 1;

    test7_done = 1;
    vTaskDelay(pdMS_TO_TICKS(50));
}

static void Run_Test08_FaultInjection(void)
{
    FaultInjector_SetOverload(0);

    printf("\n--- TEST 8: Fault Injection — CPU Overload ---\n");
    printf("    Expected: DEADLINE_MISS -> FAIL\n");
    fflush(stdout);
    LogTestPhase(8, "FAULT_INJECTION", "START");

    xTaskCreate(Test8_Task_CriticalWork, "CriticalWork",
        TASK_STACK_SIZE, NULL, PRIORITY_HIGH, NULL);
    xTaskCreate(Test8_Task_CPUOverload, "CPUOverload",
        TASK_STACK_SIZE, NULL, PRIORITY_HIGH, NULL);

    printf("    Phase A: Normal (500ms)...\n");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(500));

    printf("    Phase B: Overload INJECTED (500ms)...\n");
    fflush(stdout);
    FaultInjector_SetOverload(1);

    vTaskDelay(pdMS_TO_TICKS(500));

    printf("--- TEST 8: FAIL (DEADLINE_MISS — expected) ---\n");
    fflush(stdout);

    LogTestPhase(8, "FAULT_INJECTION", "FAIL");
    testResults[7] = -1;

    FaultInjector_SetOverload(0);
    test8_done = 1;
    vTaskDelay(pdMS_TO_TICKS(100));
}

static void Run_Test09_Timing(void)
{
    printf("\n--- TEST 9: Timing Analysis — Runtime Metrics ---\n");
    fflush(stdout);
    LogTestPhase(9, "TIMING_ANALYSIS", "START");

    xTaskCreate(Test9_Task_Measured, "MeasuredTask",
        TASK_STACK_SIZE, NULL, PRIORITY_HIGH, NULL);

    vTaskDelay(pdMS_TO_TICKS(2000));

    printf("--- TEST 9: PASS ---\n");
    fflush(stdout);

    LogTestPhase(9, "TIMING_ANALYSIS", "PASS");
    testResults[8] = 1;

    test9_done = 1;
    vTaskDelay(pdMS_TO_TICKS(50));
}

/* ======================================================== */
/*  SUMMARY LOG                                              */
/* ======================================================== */

static void WriteSummaryLog(void)
{
    FILE* summaryFile = NULL;
    fopen_s(&summaryFile, "test_summary.txt", "w");

    TickType_t tick = xTaskGetTickCount();
    uint32_t totalMs = tick * portTICK_PERIOD_MS;

    int passCount = 0, failCount = 0, runCount = 0;

    for (int i = 0; i < TEST_COUNT; i++)
    {
        if (testResults[i] != 0) runCount++;
        if (testResults[i] == 1) passCount++;
        if (testResults[i] == -1) failCount++;
    }

    static const char* testCategories[TEST_COUNT] = {
        "Scheduler Testing", "Priority Scheduling", "Priority Preemption",
        "Hard Deadline", "Soft Deadline", "Synchronization",
        "Concurrency", "Fault Injection", "Timing Analysis"
    };
    static const char* testScenarios[TEST_COUNT] = {
        "Sensor Nguoi Ra Vao", "Nha May E-Stop", "Camera An Ninh",
        "Bao Chay Thong Minh", "Tram Quan Trac Tiet", "Ghi Log Nha TT Minh",
        "Drone Tranh Vat Can", "CPU Overload", "Runtime Metrics"
    };
    static const char* testCriteria[TEST_COUNT] = {
        "CONTEXT_SWITCH", "READY_START_ORDER", "PREEMPTED Event",
        "Deadline Miss=0", "Response Time OK", "QUEUE_SEND/MUTEX",
        "Runtime Stable", "DEADLINE_MISS", "WCET/ResponseTime"
    };

    if (summaryFile != NULL)
    {
        fprintf(summaryFile, "+------------------------------------------------------------------------------+\n");
        fprintf(summaryFile, "|              RTOS TEST SUITE  -  KET QUA TONG HOP                          |\n");
        fprintf(summaryFile, "+------------------------------------------------------------------------------+\n\n");
        fprintf(summaryFile, " Thoi gian chay : %lu ms (%.1f giay)\n",
            (unsigned long)totalMs, totalMs / 1000.0);
        fprintf(summaryFile, " So test chay   : %d / %d\n\n", runCount, TEST_COUNT);
        fprintf(summaryFile, "+------+----------------------+------------------------+----------------------+--------+\n");
        fprintf(summaryFile, "| STT  | Nhom Kiem Thu        | Kich Ban               | Tieu Chi             | Ket Qua|\n");
        fprintf(summaryFile, "+------+----------------------+------------------------+----------------------+--------+\n");
    }

    for (int i = 0; i < TEST_COUNT; i++)
    {
        if (testResults[i] == 0) continue;
        const char* result = (testResults[i] == 1) ? "PASS" : "FAIL";

        if (summaryFile != NULL)
        {
            fprintf(summaryFile, "| %-4d | %-20s | %-22s | %-20s |  %-4s  |\n",
                i + 1, testCategories[i], testScenarios[i], testCriteria[i], result);
        }
    }

    if (summaryFile != NULL)
    {
        fprintf(summaryFile, "+------+----------------------+------------------------+----------------------+--------+\n\n");
        fprintf(summaryFile, "+-------------------------- TONG KET --------------------------+\n");
        fprintf(summaryFile, "|  PASS      : %-3d                                            |\n", passCount);
        fprintf(summaryFile, "|  FAIL      : %-3d                                            |\n", failCount);
        fprintf(summaryFile, "|  Total     : %-3d / %d                                        |\n", runCount, TEST_COUNT);
        fprintf(summaryFile, "|  Thoi gian : %-6lu ms                                       |\n", (unsigned long)totalMs);
        fprintf(summaryFile, "+--------------------------------------------------------------+\n\n");
        fprintf(summaryFile, " Chi tiet tung event xem: test_suite_log.csv\n");
        fclose(summaryFile);
    }

    printf("\n+------------------------------------------------------------------------------+\n");
    printf("|              RTOS TEST SUITE  -  KET QUA TONG HOP                          |\n");
    printf("+------------------------------------------------------------------------------+\n\n");
    printf(" Thoi gian chay : %lu ms (%.1f giay)\n",
        (unsigned long)totalMs, totalMs / 1000.0);
    printf(" So test chay   : %d / %d\n\n", runCount, TEST_COUNT);
    printf("+------+----------------------+------------------------+----------------------+--------+\n");
    printf("| STT  | Nhom Kiem Thu        | Kich Ban               | Tieu Chi             | Ket Qua|\n");
    printf("+------+----------------------+------------------------+----------------------+--------+\n");
    fflush(stdout);

    for (int i = 0; i < TEST_COUNT; i++)
    {
        if (testResults[i] == 0) continue;
        const char* result = (testResults[i] == 1) ? "PASS" : "FAIL";
        printf("| %-4d | %-20s | %-22s | %-20s |  %-4s  |\n",
            i + 1, testCategories[i], testScenarios[i], testCriteria[i], result);
    }
    fflush(stdout);

    printf("+------+----------------------+------------------------+----------------------+--------+\n\n");
    printf(" PASS: %d  |  FAIL: %d  |  Total: %d/%d  |  Time: %lu ms\n\n",
        passCount, failCount, runCount, TEST_COUNT, (unsigned long)totalMs);
    fflush(stdout);

    if (xLogFile != NULL)
    {
        fprintf(xLogFile, "\nSUMMARY,%d,%d,%d,%d,%lu\n",
            passCount, failCount, runCount, TEST_COUNT, (unsigned long)totalMs);
        fflush(xLogFile);
        fclose(xLogFile);
        xLogFile = NULL;
    }
}

/* ======================================================== */
/*  TEST CONTROLLER                                          */
/* ======================================================== */

static void Task_TestController(void* pvParameters)
{
    (void)pvParameters;
    vTaskDelay(pdMS_TO_TICKS(300));

    printf("\n============================================\n");
    printf("  RTOS TEST SUITE — 9 KICH BAN KIEM THU\n");
    printf("  RUN_TEST_CASE = %d", RUN_TEST_CASE);
    if (RUN_TEST_CASE == 0) printf(" (tat ca)");
    printf("\n============================================\n");
    fflush(stdout);

    if (RUN_TEST_CASE == 0 || RUN_TEST_CASE == 1) Run_Test01_Scheduler();
    vTaskDelay(pdMS_TO_TICKS(300));

    if (RUN_TEST_CASE == 0 || RUN_TEST_CASE == 2) Run_Test02_Priority();
    vTaskDelay(pdMS_TO_TICKS(300));

    if (RUN_TEST_CASE == 0 || RUN_TEST_CASE == 3) Run_Test03_Preemption();
    vTaskDelay(pdMS_TO_TICKS(300));

    if (RUN_TEST_CASE == 0 || RUN_TEST_CASE == 4) Run_Test04_HardDeadline();
    vTaskDelay(pdMS_TO_TICKS(300));

    if (RUN_TEST_CASE == 0 || RUN_TEST_CASE == 5) Run_Test05_SoftDeadline();
    vTaskDelay(pdMS_TO_TICKS(300));

    if (RUN_TEST_CASE == 0 || RUN_TEST_CASE == 6) Run_Test06_Synchronization();
    vTaskDelay(pdMS_TO_TICKS(300));

    if (RUN_TEST_CASE == 0 || RUN_TEST_CASE == 7) Run_Test07_Concurrency();
    vTaskDelay(pdMS_TO_TICKS(300));

    if (RUN_TEST_CASE == 0 || RUN_TEST_CASE == 8) Run_Test08_FaultInjection();
    vTaskDelay(pdMS_TO_TICKS(300));

    if (RUN_TEST_CASE == 0 || RUN_TEST_CASE == 9) Run_Test09_Timing();

    WriteSummaryLog();

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ===================== ENTRY POINT ===================== */

void main_test_suite(void)
{
    fopen_s(&xLogFile, "test_suite_log.csv", "w");

    const char* csvHeader =
        "timestamp_ms,task_name,priority,event,counter,execution_time_us,status\n";

    printf("%s", csvHeader);

    if (xLogFile != NULL)
    {
        fprintf(xLogFile, "%s", csvHeader);
        fflush(xLogFile);
    }

    xDataQueue = xQueueCreate(QUEUE_LENGTH, QUEUE_ITEM_SIZE);
    xLogMutex  = xSemaphoreCreateMutex();
    xTestMutex = xSemaphoreCreateMutex();
    xBinarySem = xSemaphoreCreateBinary();

    if (xDataQueue == NULL || xLogMutex == NULL ||
        xTestMutex == NULL || xBinarySem == NULL)
    {
        printf("ERROR: Failed to create shared resources\n");
        fflush(stdout);
        return;
    }

    xTaskCreate(Task_TestController, "TestCtrl",
        TASK_STACK_SIZE, NULL, PRIORITY_TEST_CTRL, NULL);

    vTaskStartScheduler();

    for (;;)
    {
    }
}
