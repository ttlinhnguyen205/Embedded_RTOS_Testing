# Embedded_RTOS_Testing

## Xây dựng và Kiểm Thử Hệ Nhúng Đa Tiến Trình Thời Gian Thực Sử Dụng FreeRTOS

Dự án tập trung nghiên cứu, xây dựng và kiểm thử một hệ thống nhúng đa tiến trình thời gian thực sử dụng **FreeRTOS Simulator**.

Mục tiêu của đề tài là đánh giá hành vi của **Scheduler**, **Priority Scheduling**, **Priority Preemption**, **Synchronization**, **Deadline Analysis** và **Fault Testing** trong môi trường hệ điều hành thời gian thực.

---

## Tổng quan đề tài

Trong hệ thống nhúng thời gian thực, nhiều task phải hoạt động song song và cạnh tranh tài nguyên CPU. Việc kiểm thử cơ chế lập lịch, ưu tiên task, đồng bộ hóa và deadline là yếu tố quan trọng để đảm bảo hệ thống vận hành ổn định.

Dự án triển khai nhiều task mô phỏng hoạt động thực tế và thực hiện kiểm thử dựa trên các tiêu chí:

* Scheduler Testing
* Priority Scheduling
* Priority Preemption
* Hard / Soft Deadline
* Synchronization
* Concurrency Testing
* Fault Injection
* Timing Analysis
* Pass / Fail Evaluation

---

## Môi trường phát triển

* Ngôn ngữ: C
* RTOS: FreeRTOS
* IDE: VSCode / Visual Studio
* Nền tảng: Windows / Linux
* Compiler: GCC / MinGW
* FreeRTOS Simulator

---

## Kiến trúc hệ thống

Hệ thống bao gồm nhiều task chạy đồng thời:

| Task                  | Chức năng                 | Priority | Deadline |
| --------------------- | ------------------------- | -------- | -------- |
| Sensor_Task           | Thu thập dữ liệu cảm biến | 2        | 100 ms   |
| Control_Task          | Xử lý điều khiển          | 4        | 50 ms    |
| Communication_Task    | Truyền thông dữ liệu      | 3        | 150 ms   |
| Logger_Task           | Ghi log hệ thống          | 1        | 200 ms   |
| Emergency_Task        | Xử lý khẩn cấp            | 5        | 20 ms    |
| Deadline_Monitor_Task | Giám sát deadline         | 4        | 100 ms   |
| Fault_Injector_Task   | Sinh lỗi kiểm thử         | 1        | 500 ms   |

Các cơ chế RTOS được sử dụng:

* Task Scheduling
* Queue Communication
* Mutex Protection
* Semaphore Synchronization
* Deadline Monitoring
* Fault Injection

---

## Các tính năng chính

### 1. Scheduler Testing

Kiểm thử cơ chế lập lịch của FreeRTOS:

* Priority Scheduling
* Round Robin
* Context Switching
* Task State Transition

### 2. Priority Preemption

Mô phỏng trường hợp:

* Low Priority Task đang chạy.
* Emergency Task xuất hiện.
* Scheduler preempt task ưu tiên thấp.
* Emergency Task chiếm CPU ngay lập tức.

### 3. Synchronization

Thực hiện kiểm thử:

* Mutex
* Binary Semaphore
* Queue
* Shared Resource Protection

### 4. Timing Analysis

Thu thập và phân tích:

* Execution Time
* Response Time
* Waiting Time
* WCET
* Deadline Miss Rate
* CPU Usage
* Context Switch Count

### 5. Fault Testing

Mô phỏng lỗi:

* Task Crash
* Timeout
* CPU Overload
* Faulty Sensor
* Starvation
* Deadline Miss

---

## Cấu trúc thư mục dự án

```text
Embedded_RTOS_Testing/
│
├── src/
│   ├── main.c
│   ├── tasks.c
│   ├── scheduler.c
│   ├── logger.c
│   └── deadline_monitor.c
│
├── include/
│   ├── config.h
│   ├── tasks.h
│   └── metrics.h
│
├── test_results/
│   ├── logs/
│   ├── csv/
│   └── graphs/
│
├── screenshots/
│
├── docs/
│   └── report.pdf
│
└── README.md
```

---

## Hướng dẫn build và chạy

### Clone project

```bash
git clone https://github.com/ttlinhnguyen205/Embedded_RTOS_Testing.git
```

### Build

```bash
cd Embedded_RTOS_Testing

mkdir build

cd build

cmake ..

make
```

### Run

```bash
./Embedded_RTOS_Testing
```

---

## Kết quả kiểm thử

Ví dụ kết quả thực nghiệm:

| Task               | Avg Response | WCET  | Deadline | Miss Count | Result |
| ------------------ | ------------ | ----- | -------- | ---------- | ------ |
| Sensor_Task        | 26.67 ms     | 9 ms  | 100 ms   | 0          | PASS   |
| Control_Task       | 0.42 ms      | 22 ms | 50 ms    | 0          | PASS   |
| Communication_Task | 26.00 ms     | 14 ms | 150 ms   | 0          | PASS   |
| Emergency_Task     | 0 ms         | 9 ms  | 20 ms    | 0          | PASS   |
| Logger_Task        | 58.33 ms     | 54 ms | 200 ms   | 0          | PASS   |

Kết quả cho thấy:

* Scheduler hoạt động đúng theo priority.
* Emergency Task preempt thành công.
* Không xuất hiện deadline miss.
* Cơ chế Mutex giải quyết race condition hiệu quả.

---

## Hình ảnh minh họa

### Console Output

![output](screenshots/output.png)

### Timing Analysis

![timing](screenshots/timing_chart.png)

### Gantt Chart

![gantt](screenshots/gantt_chart.png)

---

## Kiến thức đạt được

Thông qua dự án này, nhóm thực hiện đã nghiên cứu:

* Kiến trúc RTOS
* FreeRTOS Scheduler
* Priority Scheduling
* Synchronization Mechanisms
* Deadline Analysis
* Embedded Software Testing
* Fault Injection Techniques

---

## Hướng phát triển

Trong tương lai dự án có thể mở rộng:

* Triển khai trên STM32 thực tế.
* Thử nghiệm trên ESP32.
* UART Communication thực.
* Dashboard visualization.
* Time-series metrics analysis.
* AI anomaly detection.

---

