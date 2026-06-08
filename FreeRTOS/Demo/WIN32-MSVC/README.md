# Embedded_RTOS_Testing

## Xây dựng và Kiểm Thử Hệ Nhúng Đa Tiến Trình Thời Gian Thực Sử Dụng FreeRTOS

Dự án nghiên cứu, xây dựng và kiểm thử hệ thống nhúng đa tiến trình thời gian thực trên **FreeRTOS Win32 Simulator**.

Mục tiêu: đánh giá hành vi của **Scheduler**, **Priority Scheduling**, **Priority Preemption**, **Synchronization**, **Deadline Analysis**, **Fault Testing** và **Timing Analysis** trong môi trường RTOS.

---

## Tổng quan đề tài

Trong hệ thống nhúng thời gian thực, nhiều task hoạt động song song và cạnh tranh CPU. Kiểm thử cơ chế lập lịch, ưu tiên, đồng bộ hóa và deadline là yếu tố then chốt để đảm bảo hệ thống vận hành ổn định.

Dự án triển khai 9 kịch bản kiểm thử, mỗi kịch bản mô phỏng một tình huống thực tế:

| # | Nhóm kiểm thử       | Kịch bản mô phỏng        | Kết quả kỳ vọng |
| - | -------------------- | ------------------------ | --------------- |
| 1 | Scheduler Testing    | Sensor Kiểm Soát Người Ra Vào | PASS |
| 2 | Priority Scheduling  | Nhà Máy Emergency Stop   | PASS |
| 3 | Priority Preemption  | Camera An Ninh           | PASS |
| 4 | Hard Deadline        | Báo Cháy Thông Minh      | PASS |
| 5 | Soft Deadline        | Trạm Quan Trắc Thời Tiết | PASS |
| 6 | Synchronization      | Ghi Log (Queue + Mutex)  | PASS |
| 7 | Concurrency          | Drone Tránh Vật Cản      | PASS |
| 8 | Fault Injection      | CPU Overload             | FAIL |
| 9 | Timing Analysis      | Runtime Metrics          | PASS |

---

## Môi trường phát triển

* Ngôn ngữ: C (C99)
* RTOS: FreeRTOS v10.1.1
* Port: WIN32-MSVC (simulator chạy trên Windows)
* IDE / Build: Visual Studio + MSBuild (`.vcxproj`)
* Platform Toolset: v145 (Visual Studio 18 / 2022)
* Hệ điều hành: Windows

---

## Kiến trúc hệ thống

Toàn bộ bộ kiểm thử nằm trong một file đơn `main_test_suite.c`, được gọi từ `main.c` qua hàm `main_test_suite()` (khi `mainCREATE_SIMPLE_BLINKY_DEMO_ONLY = 3`).

```
main.c
  └── main_test_suite()          (main_test_suite.c)
        ├── Khởi tạo: log file, queue, mutex, semaphore
        ├── Task_TestController  (PRIORITY_TEST_CTRL = 5)
        │     ├── Run_Test01_Scheduler
        │     ├── Run_Test02_Priority
        │     ├── Run_Test03_Preemption
        │     ├── Run_Test04_HardDeadline
        │     ├── Run_Test05_SoftDeadline
        │     ├── Run_Test06_Synchronization
        │     ├── Run_Test07_Concurrency
        │     ├── Run_Test08_FaultInjection
        │     ├── Run_Test09_Timing
        │     └── WriteSummaryLog
        └── 9 nhóm task (Test1..Test9)
```

### Task priorities

| Priority | Giá trị | Ví dụ |
| -------- | ------- | ----- |
| LOW      | 1       | FactoryLow, Logger |
| MEDIUM   | 2       | DoorSensor, VisitorCounter, CameraScan, TempSensor |
| HIGH     | 3       | FactoryHigh, Navigation, CriticalWork |
| CRITICAL | 4       | IntrusionAlert, SmokeSensor |
| TEST_CTRL| 5       | Task_TestController |

### Cơ chế RTOS sử dụng

* `xTaskCreate` / `vTaskDelete` — tạo & dọn task
* `xQueueSend` / `xQueueReceive` — giao tiếp producer-consumer
* `xSemaphoreCreateMutex` — bảo vệ tài nguyên chung (log, biến chia sẻ)
* `xSemaphoreCreateBinary` — báo hiệu interrupt-style (IntrusionAlert)
* `xTaskNotifyGive` / `ulTaskNotifyTake` — trigger task theo thứ tự (Test 2)
* `vTaskDelay` — định kỳ task
* `portGET_RUN_TIME_COUNTER_VALUE` — đo WCET

---

## Các tính năng chính

### 1. Scheduler Testing (Test 1)
DoorSensor và VisitorCounter chạy cùng priority MEDIUM — kiểm tra context switching round-robin.

### 2. Priority Scheduling (Test 2)
FactoryHigh/Med/Low đợi notification, được `xTaskNotifyGive` cùng lúc — chạy theo thứ tự ưu tiên giảm dần.

### 3. Priority Preemption (Test 3)
CameraScan (MEDIUM) đang chạy; IntrusionAlert (CRITICAL) được đánh thức qua binary semaphore — preempt ngay lập tức.

### 4. Hard Deadline (Test 4)
SmokeSensor (CRITICAL) đo exec time mỗi chu kỳ, kiểm tra với hard deadline 50000 µs.

### 5. Soft Deadline (Test 5)
TempSensor + WindSensor (MEDIUM) với workload ngẫu nhiên — kiểm tra soft deadline 40000 µs.

### 6. Synchronization (Test 6)
EventProducer gửi int qua queue; LogConsumer nhận và dùng mutex để ghi log — tránh race condition.

### 7. Concurrency (Test 7)
FrontSensor, RearSensor, Navigation (HIGH) chạy song song; Navigation đọc khoảng cách từ FrontSensor qua mutex và quyết định tránh vật cản.

### 8. Fault Injection (Test 8)
CriticalWork + CPUOverload (HIGH). Giai đoạn A: chạy bình thường. Giai đoạn B: bật overload → CPUOverload tiêu thụ CPU → CriticalWork miss deadline (kỳ vọng FAIL).

### 9. Timing Analysis (Test 9)
MeasuredTask đo WCET và response time qua `portGET_RUN_TIME_COUNTER_VALUE` và `xTaskGetTickCount`.

---

## Cấu trúc thư mục dự án

```text
FreeRTOSv10.1.1/
│
├── FreeRTOS/
│   ├── Source/                       # Kernel FreeRTOS (tasks.c, queue.c, ...)
│   └── Demo/
│       └── WIN32-MSVC/               # ← Thư mục chính của đề tài
│           ├── main.c                # Entry point
│           ├── main_test_suite.c     # Toàn bộ bộ kiểm thử (9 kịch bản)
│           ├── FreeRTOSConfig.h      # Cấu hình RTOS
│           ├── WIN32.vcxproj         # Project Visual Studio
│           ├── WIN32.vcxproj.filters
│           ├── Run-time-stats-utils.c
│           └── README.md             # File này
│
├── FreeRTOS-Plus/                    # Trace recorder (tùy chọn)
└── README.md
```

### Output khi chạy

* `test_suite_log.csv` — chi tiết từng event (timestamp, task, priority, event, counter, exec_time, status)
* `test_summary.txt` — bảng tổng hợp PASS/FAIL của 9 test

---

## Hướng dẫn build và chạy

### Yêu cầu

* Visual Studio 2022 (hoặc Build Tools) với workload *Desktop development with C++*
* Platform Toolset v145

### Build bằng Visual Studio

1. Mở `FreeRTOS/Demo/WIN32-MSVC/WIN32.vcxproj`
2. Build (Ctrl+Shift+B) — cấu hình Debug | Win32

### Build bằng command line

```bash
cd FreeRTOS/Demo/WIN32-MSVC

MSBuild.exe WIN32.vcxproj /p:Configuration=Debug /p:Platform=Win32 /t:Build
```

### Chạy

```bash
./Debug/RTOSDemo.exe
```

Kết quả in ra console và ghi vào `test_suite_log.csv`. Sau khi 9 test hoàn tất, `test_summary.txt` được tạo.

### Chạy riêng một test

Trong `main_test_suite.c`, sửa:

```c
#define RUN_TEST_CASE 0    // 0 = chạy tất cả, 1..9 = chạy test cụ thể
```

---

## Kết quả kiểm thử thực tế

```
+------+----------------------+------------------------+----------------------+--------+
| STT  | Nhom Kiem Thu        | Kich Ban               | Tieu Chi             | Ket Qua|
+------+----------------------+------------------------+----------------------+--------+
| 1    | Scheduler Testing    | Sensor Nguoi Ra Vao    | CONTEXT_SWITCH       |  PASS  |
| 2    | Priority Scheduling  | Nha May E-Stop         | READY_START_ORDER    |  PASS  |
| 3    | Priority Preemption  | Camera An Ninh         | PREEMPTED Event      |  PASS  |
| 4    | Hard Deadline        | Bao Chay Thong Minh    | Deadline Miss=0      |  PASS  |
| 5    | Soft Deadline        | Tram Quan Trac Tiet    | Response Time OK     |  PASS  |
| 6    | Synchronization      | Ghi Log Nha TT Minh    | QUEUE_SEND/MUTEX     |  PASS  |
| 7    | Concurrency          | Drone Tranh Vat Can    | Runtime Stable       |  PASS  |
| 8    | Fault Injection      | CPU Overload           | DEADLINE_MISS        |  FAIL  |
| 9    | Timing Analysis      | Runtime Metrics        | WCET/ResponseTime    |  PASS  |
+------+----------------------+------------------------+----------------------+--------+

PASS: 8  |  FAIL: 1  |  Total: 9/9
```

Nhận xét:

* Scheduler hoạt động đúng theo priority.
* Emergency/Intrusion task preempt thành công.
* Mutex bảo vệ tài nguyên chung (log, biến chia sẻ) hiệu quả.
* Test 8 xác nhận: khi CPU overload, critical task miss deadline — đúng kỳ vọng để chứng minh cơ chế deadline monitor hoạt động.

---

## Kiến thức đạt được

* Kiến trúc RTOS và FreeRTOS Scheduler
* Priority Scheduling & Preemption
* Đồng bộ hóa: Mutex, Semaphore, Queue, Task Notification
* Hard / Soft Deadline Monitoring
* Fault Injection Testing
* WCET và Response Time Measurement
* Cấu hình và build FreeRTOS trên Windows (Win32-MSVC port)

---

## Hướng phát triển

* Port sang STM32 / ESP32 phần cứng thực
* Thêm UART communication với sensor thật
* Dashboard visualization (web hoặc GUI)
* Phân tích time-series metrics với Python
* AI anomaly detection cho runtime behavior
