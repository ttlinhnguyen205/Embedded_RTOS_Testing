#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Phan tich metrics tu test_suite_log.csv
========================================
Xuat 2 file:
  - metrics_baseline.csv : chi so hoat dong binh thuong (Test 1-7, 9)
  - metrics_fault.csv    : chi so khi fault injection (Test 8)

Cach dung:
  python analyze_metrics.py
"""

import csv
import os
from collections import defaultdict

LOG_FILE = "test_suite_log.csv"
BASELINE_OUT = "metrics_baseline.csv"
FAULT_OUT = "metrics_fault.csv"

# Test boundaries (timestamp ranges)
FAULT_TESTS = {8}  # Test 8 = Fault Injection
BASELINE_TESTS = {1, 2, 3, 4, 5, 6, 7, 9}


def read_log(path):
    rows = []
    if not os.path.exists(path):
        print(f"[!] Khong tim thay {path}. Hay chay test suite truoc.")
        return rows

    with open(path, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            task_name = (row.get("task_name") or "").strip()
            if not task_name or task_name in ("SUMMARY", "timestamp_ms"):
                continue
            try:
                row["timestamp_ms"] = int(row.get("timestamp_ms", 0))
                row["priority"] = int(row.get("priority", 0))
                row["counter"] = int(row.get("counter", 0))
                row["execution_time_us"] = int(row.get("execution_time_us", 0))
            except (ValueError, TypeError):
                continue
            rows.append(row)
    return rows


def find_test_boundaries(rows):
    """Tra ve dict: test_num -> (start_ts, end_ts)."""
    boundaries = {}
    for r in rows:
        if r["task_name"] != "TEST_CTRL":
            continue
        event = r["event"]
        if event.startswith("TEST_") and event.endswith("_START"):
            pass
        parts = event.split("_")
        # TEST_1_SCHEDULER_START -> num=1
        if parts[0] == "TEST":
            try:
                num = int(parts[1])
            except (ValueError, IndexError):
                continue
            if r["status"] == "START":
                boundaries.setdefault(num, {})["start"] = r["timestamp_ms"]
            else:
                boundaries.setdefault(num, {})["end"] = r["timestamp_ms"]
    return {k: (v["start"], v["end"]) for k, v in boundaries.items()
            if "start" in v and "end" in v}


def get_test_for_event(event_row, test_spans):
    """Xac dinh event thuoc test nao dua tren timestamp va task context."""
    ts = event_row["timestamp_ms"]
    task = event_row["task_name"]

    # thu tu moi test: task nao thuoc test nao
    test_tasks = {
        1: {"DoorSensor", "VisitorCounter"},
        2: {"FactoryHigh", "FactoryMed", "FactoryLow"},
        3: {"CameraScan", "IntrusionAlert"},
        4: {"SmokeSensor"},
        5: {"TempSensor", "WindSensor"},
        6: {"EventProducer", "LogConsumer"},
        7: {"FrontSensor", "RearSensor", "Navigation"},
        8: {"CriticalWork", "CPUOverload"},
        9: {"MeasuredTask"},
    }

    for test_num, tasks in test_tasks.items():
        if task not in tasks:
            continue
        if test_num in test_spans:
            start_ts, end_ts = test_spans[test_num]
            # Cho phep event nam trong [start, end + 500ms buffer]
            if start_ts <= ts <= end_ts + 500:
                return test_num
    return None


def compute_metrics(rows, boundaries, test_nums, label):
    """Tinh toan chi so cho cac test thuoc test_nums."""
    metrics = []

    for test_num in sorted(test_nums):
        if test_num not in boundaries:
            continue
        start_ts, end_ts = boundaries[test_num]

        test_rows = [r for r in rows
                     if r["task_name"] != "TEST_CTRL"
                     and get_test_for_event(r, boundaries) == test_num]

        if not test_rows:
            continue

        # Group by task
        by_task = defaultdict(list)
        for r in test_rows:
            by_task[r["task_name"]].append(r)

        duration_ms = end_ts - start_ts

        for task_name, task_rows in sorted(by_task.items()):
            events = [r["event"] for r in task_rows]
            exec_times = [r["execution_time_us"] for r in task_rows
                          if r["execution_time_us"] > 0]
            statuses = [r["status"] for r in task_rows]
            pass_count = statuses.count("PASS")
            fail_count = statuses.count("FAIL")
            stress_count = statuses.count("STRESS")
            warning_count = sum(1 for s in statuses if "WARN" in s)

            # Inter-arrival time (khoang giua 2 event lien tiep)
            timestamps = [r["timestamp_ms"] for r in task_rows]
            intervals = []
            for i in range(1, len(timestamps)):
                intervals.append(timestamps[i] - timestamps[i-1])

            event_counter = defaultdict(int)
            for e in events:
                event_counter[e] += 1

            row = {
                "label": label,
                "test_num": test_num,
                "task_name": task_name,
                "priority": task_rows[0]["priority"],
                "duration_ms": duration_ms,
                "total_events": len(task_rows),
                "events_per_sec": round(len(task_rows) / (duration_ms / 1000.0), 2)
                                   if duration_ms > 0 else 0,
                "pass_count": pass_count,
                "fail_count": fail_count,
                "stress_count": stress_count,
                "warning_count": warning_count,
                "pass_rate_pct": round(pass_count * 100.0 / len(task_rows), 1),
                "exec_min_us": min(exec_times) if exec_times else 0,
                "exec_max_us": max(exec_times) if exec_times else 0,
                "exec_avg_us": round(sum(exec_times) / len(exec_times), 1)
                               if exec_times else 0,
                "interval_min_ms": min(intervals) if intervals else 0,
                "interval_max_ms": max(intervals) if intervals else 0,
                "interval_avg_ms": round(sum(intervals) / len(intervals), 1)
                                   if intervals else 0,
                "top_event": max(event_counter, key=event_counter.get)
                             if event_counter else "",
                "top_event_count": max(event_counter.values())
                                   if event_counter else 0,
            }
            metrics.append(row)

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


def main():
    rows = read_log(LOG_FILE)
    if not rows:
        return

    print(f"[*] Doc duoc {len(rows)} events tu {LOG_FILE}")

    boundaries = find_test_boundaries(rows)
    print(f"[*] Tim thay {len(boundaries)} test boundaries: "
          f"{sorted(boundaries.keys())}")

    # Baseline: Test 1-7, 9 (khong fault injection)
    baseline = compute_metrics(rows, boundaries, BASELINE_TESTS, "baseline")
    write_csv(baseline, BASELINE_OUT)

    # Fault: Test 8 (CPU overload injection)
    fault = compute_metrics(rows, boundaries, FAULT_TESTS, "fault_injection")
    write_csv(fault, FAULT_OUT)

    # Tong ket nhanh
    print()
    print("=== TONG KET ===")
    if baseline:
        total_baseline_events = sum(m["total_events"] for m in baseline)
        avg_pass_rate = sum(m["pass_rate_pct"] for m in baseline) / len(baseline)
        print(f"  Baseline      : {len(baseline)} task-metrics, "
              f"{total_baseline_events} events, "
              f"avg pass rate: {avg_pass_rate:.1f}%")
    if fault:
        total_fault_events = sum(m["total_events"] for m in fault)
        total_stress = sum(m["stress_count"] for m in fault)
        print(f"  Fault Inject  : {len(fault)} task-metrics, "
              f"{total_fault_events} events, "
              f"stress events: {total_stress}")

    print()
    print("Hoan thanh!")


if __name__ == "__main__":
    main()
