#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Phan tich metrics tu test_suite_log.csv
Xuat:
    - metrics_baseline.csv : Test 1-7 va Test 9
    - metrics_fault.csv    : Test 8 Fault Injection

Cach dung:
    python analyze_metrics.py
"""

import csv
import os
import re
from collections import defaultdict, Counter

LOG_FILE = "test_suite_log.csv"
BASELINE_OUT = "metrics_baseline.csv"
FAULT_OUT = "metrics_fault.csv"

BASELINE_TESTS = {1, 2, 3, 4, 5, 6, 7, 9}
FAULT_TESTS = {8}


def to_int(value, default=0):
    try:
        return int(float(value))
    except (ValueError, TypeError):
        return default


def read_log(path):
    rows = []

    if not os.path.exists(path):
        print(f"[!] Khong tim thay {path}. Hay chay test suite truoc.")
        return rows

    with open(path, "r", encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)

        for row in reader:
            task_name = (row.get("task_name") or "").strip()

            if not task_name:
                continue

            # Bo qua dong loi/header lap lai neu co
            if task_name in ("SUMMARY", "timestamp_ms"):
                continue

            row["timestamp_ms"] = to_int(row.get("timestamp_ms"))
            row["priority"] = to_int(row.get("priority"))
            row["counter"] = to_int(row.get("counter"))
            row["response_time_ms"] = to_int(row.get("response_time_ms"))
            row["execution_time_ms"] = to_int(row.get("execution_time_ms"))
            row["deadline_ms"] = to_int(row.get("deadline_ms"))
            row["task_name"] = task_name
            row["event"] = (row.get("event") or "").strip()
            row["status"] = (row.get("status") or "").strip()

            rows.append(row)

    return rows


def extract_test_num(event):
    """
    Nhan cac dang:
        TEST_01_SCHEDULER_START
        TEST_01_SCHEDULER
        TEST_08_FAULT_INJECTION
    Tra ve so test: 1..9
    """
    m = re.match(r"^TEST_(\d+)_", event)
    if not m:
        return None
    return int(m.group(1))


def find_test_boundaries(rows):
    """
    Log cua ban khong co *_END.
    Dong ket thuc test co dang:
        TEST_01_SCHEDULER,status=PASS
        TEST_08_FAULT_INJECTION,status=FAIL

    Dong bat dau co dang:
        TEST_01_SCHEDULER_START,status=START
    """
    boundaries = {}

    for r in rows:
        if r["task_name"] != "TEST_CTRL":
            continue

        event = r["event"]
        num = extract_test_num(event)

        if num is None:
            continue

        if event.endswith("_START") or r["status"].upper() == "START":
            boundaries.setdefault(num, {})["start"] = r["timestamp_ms"]
        else:
            boundaries.setdefault(num, {})["end"] = r["timestamp_ms"]
            boundaries.setdefault(num, {})["result"] = r["status"]
            boundaries.setdefault(num, {})["name"] = event

    result = {}

    for num, item in boundaries.items():
        if "start" in item and "end" in item:
            result[num] = {
                "start": item["start"],
                "end": item["end"],
                "result": item.get("result", ""),
                "name": item.get("name", f"TEST_{num:02d}"),
            }

    return result


def assign_test_num(row, boundaries):
    """Gan event vao test dua tren timestamp."""
    ts = row["timestamp_ms"]

    for num, span in sorted(boundaries.items()):
        if span["start"] <= ts <= span["end"]:
            return num

    return None


def is_deadline_miss(row):
    return row["deadline_ms"] > 0 and row["response_time_ms"] > row["deadline_ms"]


def avg(values):
    return sum(values) / len(values) if values else 0


def compute_metrics(rows, boundaries, test_nums, label):
    metrics = []

    for test_num in sorted(test_nums):
        if test_num not in boundaries:
            print(f"[!] Khong tim thay boundary cho Test {test_num}")
            continue

        span = boundaries[test_num]
        start_ts = span["start"]
        end_ts = span["end"]
        duration_ms = end_ts - start_ts

        test_rows = []
        for r in rows:
            if r["task_name"] in ("TEST_CTRL", "SYSTEM"):
                continue
            if assign_test_num(r, boundaries) == test_num:
                test_rows.append(r)

        if not test_rows:
            print(f"[!] Test {test_num}: khong co event task de tinh metrics")
            continue

        by_task = defaultdict(list)
        for r in test_rows:
            by_task[r["task_name"]].append(r)

        for task_name, task_rows in sorted(by_task.items()):
            statuses = [r["status"] for r in task_rows]
            events = [r["event"] for r in task_rows]
            timestamps = [r["timestamp_ms"] for r in task_rows]

            exec_values = [r["execution_time_ms"] for r in task_rows]
            response_values = [r["response_time_ms"] for r in task_rows]
            deadline_values = [r["deadline_ms"] for r in task_rows if r["deadline_ms"] > 0]

            intervals = []
            for i in range(1, len(timestamps)):
                intervals.append(timestamps[i] - timestamps[i - 1])

            event_counter = Counter(events)

            pass_count = sum(1 for s in statuses if "PASS" in s.upper())
            fail_count = sum(1 for s in statuses if "FAIL" in s.upper())
            stress_count = sum(1 for s in statuses if "STRESS" in s.upper())
            warning_count = sum(1 for s in statuses if "WARN" in s.upper())
            deadline_miss_count = sum(1 for r in task_rows if is_deadline_miss(r) or r["event"] == "DEADLINE_MISS")

            total_events = len(task_rows)

            metrics.append({
                "label": label,
                "test_num": test_num,
                "test_name": span["name"],
                "test_result": span["result"],
                "task_name": task_name,
                "priority": task_rows[0]["priority"],
                "duration_ms": duration_ms,
                "total_events": total_events,
                "events_per_sec": round(total_events / (duration_ms / 1000.0), 2) if duration_ms > 0 else 0,
                "pass_count": pass_count,
                "fail_count": fail_count,
                "stress_count": stress_count,
                "warning_count": warning_count,
                "deadline_miss_count": deadline_miss_count,
                "pass_rate_pct": round(pass_count * 100.0 / total_events, 1) if total_events else 0,
                "exec_min_ms": min(exec_values) if exec_values else 0,
                "exec_avg_ms": round(avg(exec_values), 2),
                "exec_max_ms_WCET": max(exec_values) if exec_values else 0,
                "response_min_ms": min(response_values) if response_values else 0,
                "response_avg_ms": round(avg(response_values), 2),
                "response_max_ms": max(response_values) if response_values else 0,
                "deadline_ms": max(deadline_values) if deadline_values else 0,
                "interval_min_ms": min(intervals) if intervals else 0,
                "interval_avg_ms": round(avg(intervals), 2),
                "interval_max_ms": max(intervals) if intervals else 0,
                "top_event": event_counter.most_common(1)[0][0] if event_counter else "",
                "top_event_count": event_counter.most_common(1)[0][1] if event_counter else 0,
            })

    return metrics


def write_csv(metrics, path):
    if not metrics:
        print(f"[!] Khong co du lieu de ghi {path}")
        return

    fieldnames = list(metrics[0].keys())

    with open(path, "w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(metrics)

    print(f"[+] Da xuat: {path} ({len(metrics)} rows)")


def print_summary(baseline, fault):
    print()
    print("=== TONG KET ===")

    if baseline:
        print(f"Baseline      : {len(baseline)} task-metrics")
        print(f"  Total events: {sum(m['total_events'] for m in baseline)}")
        print(f"  Deadline miss: {sum(m['deadline_miss_count'] for m in baseline)}")
        print(f"  Avg pass rate: {avg([m['pass_rate_pct'] for m in baseline]):.1f}%")

    if fault:
        print(f"Fault Inject  : {len(fault)} task-metrics")
        print(f"  Total events: {sum(m['total_events'] for m in fault)}")
        print(f"  Stress events: {sum(m['stress_count'] for m in fault)}")
        print(f"  Fail events: {sum(m['fail_count'] for m in fault)}")
        print(f"  Deadline miss: {sum(m['deadline_miss_count'] for m in fault)}")


def main():
    rows = read_log(LOG_FILE)

    if not rows:
        return

    print(f"[*] Doc duoc {len(rows)} events tu {LOG_FILE}")

    boundaries = find_test_boundaries(rows)
    print(f"[*] Tim thay {len(boundaries)} test boundaries: {sorted(boundaries.keys())}")

    baseline = compute_metrics(rows, boundaries, BASELINE_TESTS, "baseline")
    fault = compute_metrics(rows, boundaries, FAULT_TESTS, "fault_injection")

    write_csv(baseline, BASELINE_OUT)
    write_csv(fault, FAULT_OUT)

    print_summary(baseline, fault)

    print()
    print("Hoan thanh!")


if __name__ == "__main__":
    main()
