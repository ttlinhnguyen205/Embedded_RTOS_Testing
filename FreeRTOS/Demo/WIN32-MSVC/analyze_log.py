#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
FreeRTOS Test Suite Log Analyzer
================================

Input:
    test_suite_log.csv

Output:
    report.txt
    chart_events.png
    chart_status.png
    chart_exec_time.png
    chart_response_time.png
    chart_deadline_miss.png
    chart_timeline.png
    chart_priority.png

Compatible with:
    main_test_suite.c

CSV columns expected:
    timestamp_ms,
    task_name,
    priority,
    event,
    counter,
    response_time_ms,
    execution_time_ms,
    deadline_ms,
    status
"""

import csv
import os
from collections import Counter, defaultdict

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

plt.rcParams["font.family"] = "DejaVu Sans"


LOG_FILE = "test_suite_log.csv"
REPORT_FILE = "report.txt"


# ==========================================================
# READ LOG
# ==========================================================

def read_log(path):
    rows = []

    if not os.path.exists(path):
        print(f"[ERROR] Không tìm thấy file: {path}")
        print("Hãy chạy main_test_suite.c trước để tạo test_suite_log.csv")
        return rows

    with open(path, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)

        for row in reader:
            try:
                task_name = (row.get("task_name") or "").strip()

                if not task_name:
                    continue

                if task_name == "SUMMARY":
                    continue

                if task_name == "timestamp_ms":
                    continue

                row["timestamp_ms"] = int(row.get("timestamp_ms", 0))
                row["priority"] = int(row.get("priority", 0))
                row["counter"] = int(row.get("counter", 0))
                row["response_time_ms"] = int(row.get("response_time_ms", 0))
                row["execution_time_ms"] = int(row.get("execution_time_ms", 0))
                row["deadline_ms"] = int(row.get("deadline_ms", 0))

                row["task_name"] = task_name
                row["event"] = (row.get("event") or "").strip()
                row["status"] = (row.get("status") or "").strip()

                rows.append(row)

            except Exception:
                continue

    return rows


# ==========================================================
# METRIC FUNCTIONS
# ==========================================================

def avg(values):
    if not values:
        return 0
    return sum(values) / len(values)


def deadline_missed(row):
    return (
        row["deadline_ms"] > 0 and
        row["response_time_ms"] > row["deadline_ms"]
    )


def detect_preemption_count(rows):
    """
    Ước lượng số lần preemption dựa trên thay đổi task đang chạy.
    Nếu task trước có priority thấp hơn task sau, xem như task thấp bị preempt.
    """
    count = 0

    sorted_rows = sorted(rows, key=lambda r: r["timestamp_ms"])

    for i in range(1, len(sorted_rows)):
        prev = sorted_rows[i - 1]
        curr = sorted_rows[i]

        if prev["task_name"] != curr["task_name"]:
            if curr["priority"] > prev["priority"]:
                count += 1

    return count


# ==========================================================
# WRITE REPORT
# ==========================================================

def write_report(rows):
    total_events = len(rows)

    task_counter = Counter(r["task_name"] for r in rows)
    event_counter = Counter(r["event"] for r in rows)
    status_counter = Counter(r["status"] for r in rows)

    exec_by_task = defaultdict(list)
    response_by_task = defaultdict(list)
    deadline_by_task = defaultdict(int)
    priority_by_task = {}

    total_deadline_miss = 0

    for r in rows:
        task = r["task_name"]

        exec_by_task[task].append(r["execution_time_ms"])
        response_by_task[task].append(r["response_time_ms"])
        priority_by_task[task] = r["priority"]

        if deadline_missed(r):
            total_deadline_miss += 1
            deadline_by_task[task] += 1

    pass_count = sum(
        count for status, count in status_counter.items()
        if "PASS" in status.upper()
    )

    fail_count = sum(
        count for status, count in status_counter.items()
        if "FAIL" in status.upper()
    )

    warning_count = sum(
        count for status, count in status_counter.items()
        if "WARN" in status.upper() or "WARNING" in status.upper()
    )

    pass_rate = pass_count * 100 / total_events if total_events else 0
    fail_rate = fail_count * 100 / total_events if total_events else 0
    deadline_miss_rate = total_deadline_miss * 100 / total_events if total_events else 0

    preemption_count = detect_preemption_count(rows)

    start_ts = min(r["timestamp_ms"] for r in rows)
    end_ts = max(r["timestamp_ms"] for r in rows)
    duration = end_ts - start_ts

    with open(REPORT_FILE, "w", encoding="utf-8") as out:
        out.write("=" * 80 + "\n")
        out.write("FREE RTOS TEST SUITE ANALYSIS REPORT\n")
        out.write("=" * 80 + "\n\n")

        out.write("1. TỔNG QUAN TEST RUN\n")
        out.write("-" * 80 + "\n")
        out.write(f"Tổng số event              : {total_events}\n")
        out.write(f"Thời gian bắt đầu          : {start_ts} ms\n")
        out.write(f"Thời gian kết thúc         : {end_ts} ms\n")
        out.write(f"Tổng thời gian chạy        : {duration} ms ({duration / 1000:.2f} giây)\n")
        out.write(f"Số lần Priority Preemption : {preemption_count}\n")
        out.write("\n")

        out.write("2. PASS / FAIL EVALUATION\n")
        out.write("-" * 80 + "\n")
        out.write(f"PASS events                : {pass_count}\n")
        out.write(f"FAIL events                : {fail_count}\n")
        out.write(f"WARNING events             : {warning_count}\n")
        out.write(f"PASS rate                  : {pass_rate:.2f}%\n")
        out.write(f"FAIL rate                  : {fail_rate:.2f}%\n")
        out.write("\n")

        out.write("3. DEADLINE ANALYSIS\n")
        out.write("-" * 80 + "\n")
        out.write(f"Deadline miss              : {total_deadline_miss}\n")
        out.write(f"Deadline miss rate         : {deadline_miss_rate:.2f}%\n")
        out.write("\n")

        out.write(f"{'Task':<25}{'Deadline Miss':>15}\n")
        for task in sorted(task_counter.keys()):
            out.write(f"{task:<25}{deadline_by_task[task]:>15}\n")
        out.write("\n")

        out.write("4. TASK EVENT SUMMARY\n")
        out.write("-" * 80 + "\n")
        out.write(f"{'Task':<25}{'Priority':>10}{'Events':>10}{'Percent':>12}\n")

        for task, count in sorted(task_counter.items(), key=lambda x: -x[1]):
            pct = count * 100 / total_events if total_events else 0
            out.write(
                f"{task:<25}"
                f"{priority_by_task.get(task, 0):>10}"
                f"{count:>10}"
                f"{pct:>11.2f}%\n"
            )
        out.write("\n")

        out.write("5. EXECUTION TIME ANALYSIS (ms)\n")
        out.write("-" * 80 + "\n")
        out.write(f"{'Task':<25}{'Min':>10}{'Avg':>10}{'Max/WCET':>12}{'Samples':>10}\n")

        for task, values in sorted(exec_by_task.items()):
            out.write(
                f"{task:<25}"
                f"{min(values):>10}"
                f"{avg(values):>10.2f}"
                f"{max(values):>12}"
                f"{len(values):>10}\n"
            )
        out.write("\n")

        out.write("6. RESPONSE TIME ANALYSIS (ms)\n")
        out.write("-" * 80 + "\n")
        out.write(f"{'Task':<25}{'Min':>10}{'Avg':>10}{'Max':>10}{'Samples':>10}\n")

        for task, values in sorted(response_by_task.items()):
            out.write(
                f"{task:<25}"
                f"{min(values):>10}"
                f"{avg(values):>10.2f}"
                f"{max(values):>10}"
                f"{len(values):>10}\n"
            )
        out.write("\n")

        out.write("7. STATUS SUMMARY\n")
        out.write("-" * 80 + "\n")

        for status, count in sorted(status_counter.items(), key=lambda x: -x[1]):
            pct = count * 100 / total_events if total_events else 0
            out.write(f"{status:<25}{count:>10}{pct:>11.2f}%\n")
        out.write("\n")

        out.write("8. TOP EVENTS\n")
        out.write("-" * 80 + "\n")

        for event, count in event_counter.most_common(15):
            out.write(f"{event:<40}{count:>10}\n")

        out.write("\n")
        out.write("9. KẾT LUẬN TỰ ĐỘNG\n")
        out.write("-" * 80 + "\n")

        if fail_count == 0 and total_deadline_miss == 0:
            out.write("Kết luận: Test suite đạt yêu cầu. Không phát hiện FAIL hoặc deadline miss.\n")
        elif fail_count > 0 and total_deadline_miss == 0:
            out.write("Kết luận: Có lỗi chức năng hoặc đồng bộ, nhưng chưa phát hiện deadline miss.\n")
        elif fail_count == 0 and total_deadline_miss > 0:
            out.write("Kết luận: Hệ thống có vấn đề timing/deadline dù chưa có FAIL trực tiếp.\n")
        else:
            out.write("Kết luận: Hệ thống có cả lỗi FAIL và deadline miss, cần phân tích lại scheduler/deadline.\n")

    print(f"[OK] Đã tạo {REPORT_FILE}")


# ==========================================================
# CHARTS
# ==========================================================

def chart_events(rows):
    counter = Counter(r["task_name"] for r in rows)

    tasks = list(counter.keys())
    values = list(counter.values())

    plt.figure(figsize=(12, 6))
    bars = plt.bar(tasks, values)

    plt.xticks(rotation=45, ha="right")
    plt.ylabel("Số event")
    plt.title("Số Event Theo Task")

    for bar, value in zip(bars, values):
        plt.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height(),
            str(value),
            ha="center",
            va="bottom"
        )

    plt.tight_layout()
    plt.savefig("chart_events.png", dpi=120)
    plt.close()

    print("[OK] Đã tạo chart_events.png")


def chart_status(rows):
    counter = Counter(r["status"] for r in rows)

    if not counter:
        return

    labels = list(counter.keys())
    values = list(counter.values())

    plt.figure(figsize=(8, 8))
    plt.pie(
        values,
        labels=labels,
        autopct="%1.1f%%",
        startangle=90
    )

    plt.title("Phân Bố PASS / FAIL / WARNING")
    plt.axis("equal")
    plt.tight_layout()
    plt.savefig("chart_status.png", dpi=120)
    plt.close()

    print("[OK] Đã tạo chart_status.png")


def chart_exec_time(rows):
    data = defaultdict(list)

    for r in rows:
        data[r["task_name"]].append(r["execution_time_ms"])

    tasks = []
    mins = []
    avgs = []
    maxs = []

    for task, values in sorted(data.items()):
        tasks.append(task)
        mins.append(min(values))
        avgs.append(avg(values))
        maxs.append(max(values))

    x = range(len(tasks))
    width = 0.25

    plt.figure(figsize=(13, 6))

    plt.bar([i - width for i in x], mins, width, label="Min")
    plt.bar(x, avgs, width, label="Avg")
    plt.bar([i + width for i in x], maxs, width, label="Max/WCET")

    plt.xticks(list(x), tasks, rotation=45, ha="right")
    plt.ylabel("Execution Time (ms)")
    plt.title("Min / Avg / WCET Execution Time Theo Task")
    plt.legend()
    plt.tight_layout()
    plt.savefig("chart_exec_time.png", dpi=120)
    plt.close()

    print("[OK] Đã tạo chart_exec_time.png")


def chart_response_time(rows):
    data = defaultdict(list)

    for r in rows:
        data[r["task_name"]].append(r["response_time_ms"])

    tasks = []
    mins = []
    avgs = []
    maxs = []

    for task, values in sorted(data.items()):
        tasks.append(task)
        mins.append(min(values))
        avgs.append(avg(values))
        maxs.append(max(values))

    x = range(len(tasks))
    width = 0.25

    plt.figure(figsize=(13, 6))

    plt.bar([i - width for i in x], mins, width, label="Min")
    plt.bar(x, avgs, width, label="Avg")
    plt.bar([i + width for i in x], maxs, width, label="Max")

    plt.xticks(list(x), tasks, rotation=45, ha="right")
    plt.ylabel("Response Time (ms)")
    plt.title("Response Time Theo Task")
    plt.legend()
    plt.tight_layout()
    plt.savefig("chart_response_time.png", dpi=120)
    plt.close()

    print("[OK] Đã tạo chart_response_time.png")


def chart_deadline_miss(rows):
    counter = defaultdict(int)

    for r in rows:
        if deadline_missed(r):
            counter[r["task_name"]] += 1

    if not counter:
        counter["No Deadline Miss"] = 0

    tasks = list(counter.keys())
    values = list(counter.values())

    plt.figure(figsize=(12, 6))
    bars = plt.bar(tasks, values)

    plt.xticks(rotation=45, ha="right")
    plt.ylabel("Số lần deadline miss")
    plt.title("Deadline Miss Theo Task")

    for bar, value in zip(bars, values):
        plt.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height(),
            str(value),
            ha="center",
            va="bottom"
        )

    plt.tight_layout()
    plt.savefig("chart_deadline_miss.png", dpi=120)
    plt.close()

    print("[OK] Đã tạo chart_deadline_miss.png")


def chart_priority(rows):
    priority_by_task = {}

    for r in rows:
        priority_by_task[r["task_name"]] = r["priority"]

    tasks = list(priority_by_task.keys())
    priorities = list(priority_by_task.values())

    plt.figure(figsize=(12, 6))
    bars = plt.bar(tasks, priorities)

    plt.xticks(rotation=45, ha="right")
    plt.ylabel("Priority")
    plt.title("Priority Level Theo Task")

    for bar, value in zip(bars, priorities):
        plt.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height(),
            str(value),
            ha="center",
            va="bottom"
        )

    plt.tight_layout()
    plt.savefig("chart_priority.png", dpi=120)
    plt.close()

    print("[OK] Đã tạo chart_priority.png")


def chart_timeline(rows):
    sorted_rows = sorted(rows, key=lambda r: r["timestamp_ms"])

    tasks = sorted(set(r["task_name"] for r in sorted_rows))
    task_index = {task: i for i, task in enumerate(tasks)}

    plt.figure(figsize=(15, 7))

    for r in sorted_rows:
        x = r["timestamp_ms"]
        y = task_index[r["task_name"]]

        plt.scatter(x, y, s=15)

    plt.yticks(range(len(tasks)), tasks)
    plt.xlabel("Timestamp (ms)")
    plt.ylabel("Task")
    plt.title("Timeline Event Theo Task")
    plt.tight_layout()
    plt.savefig("chart_timeline.png", dpi=120)
    plt.close()

    print("[OK] Đã tạo chart_timeline.png")


# ==========================================================
# MAIN
# ==========================================================

def main():
    rows = read_log(LOG_FILE)

    if not rows:
        return

    print(f"[INFO] Đọc được {len(rows)} events từ {LOG_FILE}")

    write_report(rows)

    chart_events(rows)
    chart_status(rows)
    chart_exec_time(rows)
    chart_response_time(rows)
    chart_deadline_miss(rows)
    chart_priority(rows)
    chart_timeline(rows)

    print()
    print("HOÀN THÀNH PHÂN TÍCH LOG")
    print("Các file đã tạo:")
    print(" - report.txt")
    print(" - chart_events.png")
    print(" - chart_status.png")
    print(" - chart_exec_time.png")
    print(" - chart_response_time.png")
    print(" - chart_deadline_miss.png")
    print(" - chart_priority.png")
    print(" - chart_timeline.png")


if __name__ == "__main__":
    main()