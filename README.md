# Embedded_RTOS_Testing

## Xây dựng và Kiểm Thử Hệ Thống Nhúng Đa Tiến Trình Thời Gian Thực Sử Dụng FreeRTOS

Dự án nghiên cứu, xây dựng và kiểm thử hệ thống nhúng đa tiến trình thời gian thực trên nền tảng **FreeRTOS Win32 Simulator**.

Mục tiêu của đề tài là đánh giá và kiểm chứng các cơ chế quan trọng trong hệ điều hành thời gian thực (RTOS), bao gồm:

* Scheduler Testing
* Priority Scheduling
* Priority Preemption
* Synchronization
* Concurrency
* Hard Deadline
* Soft Deadline
* Fault Injection
* Timing Analysis

---

# Thành viên nhóm

| MSSV     | Họ và tên            |
| -------- | -------------------- |
| 23010633 | Nguyễn Thị Thuỳ Linh |
| 23010375 | Nguyễn Anh Quân      |
| 23010588 | Trần Thảo Vy         |
| 23010623 | Nguyễn Ngọc Minh     |

---

# Tổng quan đề tài

Trong hệ thống nhúng thời gian thực, nhiều task hoạt động đồng thời và cạnh tranh tài nguyên CPU. Việc kiểm thử các cơ chế lập lịch, ưu tiên, đồng bộ hóa và deadline đóng vai trò quan trọng trong việc đảm bảo tính đúng đắn và độ tin cậy của hệ thống.

Dự án xây dựng một bộ kiểm thử gồm 9 nhóm kịch bản mô phỏng các tình huống thực tế thường gặp trong hệ thống nhúng đa nhiệm sử dụng FreeRTOS.

| STT | Nhóm kiểm thử       | Kịch bản mô phỏng                 | Kết quả kỳ vọng    |
| --- | ------------------- | --------------------------------- | ------------------ |
| 1   | Scheduler Testing   | Sensor Kiểm Soát Người Ra Vào     | PASS               |
| 2   | Priority Scheduling | Nhà Máy Emergency Stop            | PASS               |
| 3   | Priority Preemption | Camera An Ninh Phát Hiện Xâm Nhập | PASS               |
| 4   | Hard Deadline       | Hệ Thống Báo Cháy Thông Minh      | PASS               |
| 5   | Soft Deadline       | Trạm Quan Trắc Thời Tiết          | PASS               |
| 6   | Synchronization     | Hệ Thống Ghi Log Nhà Thông Minh   | PASS               |
| 7   | Concurrency         | Drone Tránh Chướng Ngại Vật       | PASS               |
| 8   | Fault Injection     | CPU Overload                      | FAIL (có chủ đích) |
| 9   | Timing Analysis     | Runtime Metrics Measurement       | PASS               |

---

# Môi trường phát triển

* Ngôn ngữ lập trình: C (C99)
* RTOS: FreeRTOS v10.1.1
* Port: WIN32-MSVC Simulator
* IDE: Microsoft Visual Studio 2022
* Compiler: MSVC
* Build Tool: MSBuild
* Hệ điều hành: Windows 10 / Windows 11

---

# Kiến trúc hệ thống

Toàn bộ bộ kiểm thử được triển khai trong file:

```text
main_test_suite.c
```

và được gọi từ:

```text
main.c
```

thông qua hàm:

```c
main_test_suite();
```

Luồng hoạt động:

```text
main.c
   │
   ▼
main_test_suite()
   │
   ├── Scheduler Testing
   ├── Priority Scheduling
   ├── Priority Preemption
   ├── Hard Deadline
   ├── Soft Deadline
   ├── Synchronization
   ├── Concurrency
   ├── Fault Injection
   ├── Timing Analysis
   │
   ▼
Runtime Logging
   │
   ▼
Metrics Analysis
   │
   ▼
PASS / FAIL Evaluation
```

---

# Cấu hình Priority

| Priority Level  | Value |
| --------------- | ----- |
| LOW             | 1     |
| MEDIUM          | 2     |
| HIGH            | 3     |
| CRITICAL        | 4     |
| TEST_CONTROLLER | 5     |

---

# Các cơ chế FreeRTOS sử dụng

* xTaskCreate()
* vTaskDelete()
* vTaskDelay()
* xTaskNotifyGive()
* ulTaskNotifyTake()
* xQueueSend()
* xQueueReceive()
* xSemaphoreCreateMutex()
* xSemaphoreCreateBinary()
* portGET_RUN_TIME_COUNTER_VALUE()
* xTaskGetTickCount()

---

# Các kịch bản kiểm thử

## Test 01 – Scheduler Testing

Kịch bản:

Sensor Kiểm Soát Người Ra Vào

Mục tiêu:

* Kiểm tra Round Robin Scheduling
* Kiểm tra Context Switching

Kết quả kỳ vọng:

* Các task cùng priority được chia CPU công bằng

---

## Test 02 – Priority Scheduling

Kịch bản:

Nhà Máy Emergency Stop

Mục tiêu:

* Kiểm tra Scheduler thực thi task theo độ ưu tiên

Kết quả kỳ vọng:

* Task có priority cao chạy trước

---

## Test 03 – Priority Preemption

Kịch bản:

Camera An Ninh Phát Hiện Xâm Nhập

Mục tiêu:

* Kiểm tra cơ chế Priority Preemption

Kết quả kỳ vọng:

* Emergency Task chiếm CPU ngay lập tức

---

## Test 04 – Hard Deadline

Kịch bản:

Hệ Thống Báo Cháy Thông Minh

Mục tiêu:

* Đánh giá Hard Real-Time Constraint

Kết quả kỳ vọng:

* Deadline Miss = 0

---

## Test 05 – Soft Deadline

Kịch bản:

Trạm Quan Trắc Thời Tiết

Mục tiêu:

* Đánh giá Soft Real-Time Constraint

Kết quả kỳ vọng:

* Response Time trong giới hạn thiết kế

---

## Test 06 – Synchronization

Kịch bản:

Hệ Thống Ghi Log Nhà Thông Minh

Mục tiêu:

* Kiểm tra Queue
* Kiểm tra Mutex

Kết quả kỳ vọng:

* Không xảy ra Race Condition

---

## Test 07 – Concurrency

Kịch bản:

Drone Tránh Chướng Ngại Vật

Mục tiêu:

* Đánh giá khả năng thực thi đồng thời

Kết quả kỳ vọng:

* Hệ thống hoạt động ổn định

---

## Test 08 – Fault Injection

Kịch bản:

CPU Overload

Mục tiêu:

* Đánh giá khả năng phát hiện lỗi thời gian thực

Kết quả kỳ vọng:

* Critical Task bị Deadline Miss
* Hệ thống ghi nhận lỗi thành công

---

## Test 09 – Timing Analysis

Kịch bản:

Runtime Metrics Measurement

Mục tiêu:

* Đo Execution Time
* Đo Response Time
* Đo WCET

Kết quả kỳ vọng:

* Thu thập đầy đủ Runtime Metrics

---

# Cấu trúc thư mục dự án

```text
Embedded_RTOS_Testing/
│
├── Charts/
│   ├── chart_deadline_miss.png
│   ├── chart_events.png
│   ├── chart_exec_time.png
│   ├── chart_priority.png
│   ├── chart_response_time.png
│   ├── chart_status.png
│   └── chart_timeline.png
│
├── Documents/
│   ├── Testing Report.docx
│   ├── Testing Report.pdf
│   └── Meeting Minutes
│
├── logs/
│   ├── metrics_baseline.csv
│   ├── metrics_fault.csv
│   ├── test_suite_log.csv
│   └── report.txt
│
├── FreeRTOS/
│   └── Demo/
│       └── WIN32-MSVC/
│           ├── main.c
│           ├── main_test_suite.c
│           ├── FreeRTOSConfig.h
│           └── WIN32.vcxproj
│
├── FreeRTOS-Plus/
│
├── .gitignore
│
└── README.md
```

---

# Kết quả kiểm thử

| Metric                   | Kết quả            |
| ------------------------ | ------------------ |
| Scheduler Testing        | PASS               |
| Priority Scheduling      | PASS               |
| Priority Preemption      | PASS               |
| Hard Deadline Compliance | PASS               |
| Soft Deadline Compliance | PASS               |
| Synchronization          | PASS               |
| Concurrency              | PASS               |
| Timing Analysis          | PASS               |
| Fault Injection          | FAIL (có chủ đích) |

Tổng kết:

```text
PASS : 8
FAIL : 1
TOTAL: 9 TEST SUITES
```

---

# Runtime Metrics Thu Thập

* Execution Time
* Response Time
* WCET
* Context Switch Count
* Deadline Miss Count
* CPU Usage
* Queue Events
* Synchronization Events
* Runtime Status

---

# Hình ảnh và biểu đồ

Thư mục `Charts/` chứa các kết quả trực quan được sinh ra từ quá trình phân tích log:

* Execution Time Analysis
* Response Time Analysis
* Deadline Miss Analysis
* Priority Distribution
* Event Distribution
* Runtime Timeline
* PASS / FAIL Summary


# Hướng phát triển

* Triển khai trên STM32
* Triển khai trên ESP32
* Bổ sung Semaphore Testing
* Bổ sung Deadlock Detection
* Bổ sung Priority Inversion Testing
* Tích hợp Tracealyzer
* Tự động hóa phân tích log bằng Python


# Tài liệu tham khảo

[1] FreeRTOS WIN32-MSVC Simulator Demo
https://github.com/FreeRTOS/FreeRTOS/tree/main/FreeRTOS/Demo/WIN32-MSVC 

[2] Jane W. S. Liu, Real-Time Systems, Prentice Hall, 2000.

[3] Qing Li, Caroline Yao, Real-Time Concepts for Embedded Systems, CMP Books, 2003.

[4] Tammy Noergaard, Embedded Systems Architecture: A Comprehensive Guide for Engineers and Programmers, Elsevier, 2012.

[5] Microsoft Visual Studio Documentation
https://learn.microsoft.com/visualstudio

[6] Python Documentation
https://docs.python.org/3

[7] Git Documentation
https://git-scm.com/doc

[8] Tài liệu hướng dẫn đồ án FreeRTOS do giảng viên cung cấp, 2025.
