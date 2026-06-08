#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Phan tich log FreeRTOS Test Suite
=================================
Doc: test_suite_log.csv
Xuat:
  - report.txt          : bang thong ke tong hop
  - chart_events.png    : bieu do event count moi task
  - chart_timeline.png  : bieu do timeline (timeline Gantt don gian)
  - chart_status.png    : bieu do trang thai PASS/FAIL/WARNING
  - chart_exec_time.png : bieu do execution time theo task

Cach dung:
  python analyze_log.py
"""

import csv
import os
from collections import Counter, defaultdict

LOG_FILE = "test_suite_log.csv"
REPORT_FILE = "report.txt"

# === Cau hinh matplotlib khong dung GUI backend ===
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

# Font Unicode de hien thi tieng Viet
plt.rcParams["font.family"] = "DejaVu Sans"


def read_log(path):
    """Doc file CSV, tra ve danh sach dict cac row."""
    rows = []
    if not os.path.exists(path):
        print(f"[!] Khong tim thay {path}. Hay chay test suite truoc.")
        return rows

    with open(path, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            # Bo qua dong trong hoac dong SUMMARY (co the xuat hien
            # do fprintf co \n dau)
            if row is None:
                continue
            task_name = (row.get("task_name") or "").strip()
            if not task_name:
                continue
            if task_name == "SUMMARY":
                continue
            if task_name == "timestamp_ms":
                continue  # header duplicate do \n
            try:
                row["timestamp_ms"] = int(row.get("timestamp_ms", 0))
                row["priority"] = int(row.get("priority", 0))
                row["counter"] = int(row.get("counter", 0))
                row["execution_time_us"] = int(row.get("execution_time_us", 0))
            except (ValueError, TypeError):
                continue
            rows.append(row)
    return rows


def write_report(rows):
    """Viet bao cao tong hop."""
    total = len(rows)
    task_counter = Counter(r["task_name"] for r in rows)
    event_counter = Counter(r["event"] for r in rows)
    status_counter = Counter(r["status"] for r in rows)

    # Execution time theo task
    exec_by_task = defaultdict(list)
    for r in rows:
        if r["execution_time_us"] > 0:
            exec_by_task[r["task_name"]].append(r["execution_time_us"])

    with open(REPORT_FILE, "w", encoding="utf-8") as out:
        out.write("=" * 70 + "\n")
        out.write("  PHAN TICH LOG FREE RTOS TEST SUITE\n")
        out.write("=" * 70 + "\n\n")

        out.write(f"Tong so events: {total}\n")
        if rows:
            start_ts = rows[0]["timestamp_ms"]
            end_ts = max(r["timestamp_ms"] for r in rows)
            out.write(f"Thoi gian bat dau: {start_ts} ms\n")
            out.write(f"Thoi gian ket thuc: {end_ts} ms\n")
            out.write(f"Tong thoi gian: {end_ts - start_ts} ms "
                      f"({(end_ts - start_ts)/1000:.1f} giay)\n")
        out.write("\n")

        # Bang theo task
        out.write("-" * 70 + "\n")
        out.write("  SO EVENT THEO TASK\n")
        out.write("-" * 70 + "\n")
        out.write(f"{'Task':<25} {'So event':>10} {'Ty le':>10}\n")
        for task, cnt in sorted(task_counter.items(),
                                key=lambda x: -x[1]):
            pct = cnt * 100.0 / total if total else 0
            out.write(f"{task:<25} {cnt:>10} {pct:>9.1f}%\n")
        out.write("\n")

        # Bang theo status
        out.write("-" * 70 + "\n")
        out.write("  THONG KE STATUS\n")
        out.write("-" * 70 + "\n")
        for status, cnt in sorted(status_counter.items(),
                                   key=lambda x: -x[1]):
            pct = cnt * 100.0 / total if total else 0
            out.write(f"{status:<20} {cnt:>10} {pct:>9.1f}%\n")
        out.write("\n")

        # Execution time stats
        out.write("-" * 70 + "\n")
        out.write("  THONG KE EXECUTION TIME (us)\n")
        out.write("-" * 70 + "\n")
        out.write(f"{'Task':<25} {'Min':>8} {'Max':>8} {'Avg':>8} {'N':>6}\n")
        for task, times in sorted(exec_by_task.items()):
            if not times:
                continue
            out.write(f"{task:<25} {min(times):>8} {max(times):>8} "
                      f"{sum(times)//len(times):>8} {len(times):>6}\n")
        out.write("\n")

        # Top events
        out.write("-" * 70 + "\n")
        out.write("  TOP 10 EVENT PHO BIEN\n")
        out.write("-" * 70 + "\n")
        for event, cnt in event_counter.most_common(10):
            out.write(f"{event:<30} {cnt:>8}\n")

    print(f"[+] Da ghi bao cao: {REPORT_FILE}")


def chart_events_by_task(rows):
    """Bieu do cot: so event moi task."""
    task_counter = Counter(r["task_name"] for r in rows if r["task_name"] != "TEST_CTRL")
    if not task_counter:
        return

    tasks = list(task_counter.keys())
    counts = list(task_counter.values())

    plt.figure(figsize=(12, 6))
    bars = plt.bar(range(len(tasks)), counts, color="steelblue", edgecolor="black")
    plt.xticks(range(len(tasks)), tasks, rotation=45, ha="right")
    plt.ylabel("So event")
    plt.title("So Event Theo Task")
    plt.grid(axis="y", linestyle="--", alpha=0.5)

    # Ghi so len moi cot
    for bar, count in zip(bars, counts):
        plt.text(bar.get_x() + bar.get_width()/2, bar.get_height(),
                 str(count), ha="center", va="bottom", fontsize=9)

    plt.tight_layout()
    plt.savefig("chart_events.png", dpi=100)
    plt.close()
    print("[+] Da xuat: chart_events.png")


def chart_status_pie(rows):
    """Bieu do tron: phan bo status."""
    status_counter = Counter(r["status"] for r in rows if r["task_name"] != "TEST_CTRL")
    if not status_counter:
        return

    labels = list(status_counter.keys())
    sizes = list(status_counter.values())
    colors = []
    for s in labels:
        if "PASS" in s:
            colors.append("seagreen")
        elif "FAIL" in s:
            colors.append("crimson")
        elif "WARN" in s:
            colors.append("orange")
        else:
            colors.append("steelblue")

    plt.figure(figsize=(8, 8))
    plt.pie(sizes, labels=labels, colors=colors, autopct="%1.1f%%",
            startangle=90, wedgeprops=dict(edgecolor="white", linewidth=1))
    plt.title("Phan Bo Status")
    plt.axis("equal")
    plt.tight_layout()
    plt.savefig("chart_status.png", dpi=100)
    plt.close()
    print("[+] Da xuat: chart_status.png")


def chart_exec_time_by_task(rows):
    """Bieu do cot nhom: min/avg/max execution time moi task."""
    exec_by_task = defaultdict(list)
    for r in rows:
        if r["task_name"] == "TEST_CTRL":
            continue
        if r["execution_time_us"] > 0:
            exec_by_task[r["task_name"]].append(r["execution_time_us"])

    if not exec_by_task:
        return

    tasks = list(exec_by_task.keys())
    mins = [min(exec_by_task[t]) for t in tasks]
    avgs = [sum(exec_by_task[t])//len(exec_by_task[t]) for t in tasks]
    maxs = [max(exec_by_task[t]) for t in tasks]

    x = range(len(tasks))
    width = 0.25

    plt.figure(figsize=(12, 6))
    plt.bar([i - width for i in x], mins, width, label="Min", color="lightgreen")
    plt.bar(x, avgs, width, label="Avg", color="steelblue")
    plt.bar([i + width for i in x], maxs, width, label="Max", color="salmon")

    plt.xticks(list(x), tasks, rotation=45, ha="right")
    plt.ylabel("Execution time (us)")
    plt.title("Min / Avg / Max Execution Time Theo Task")
    plt.legend()
    plt.grid(axis="y", linestyle="--", alpha=0.5)
    plt.tight_layout()
    plt.savefig("chart_exec_time.png", dpi=100)
    plt.close()
    print("[+] Da xuat: chart_exec_time.png")


def chart_timeline(rows):
    """Bieu do timeline: moi chuong trinh test khi nao xuat hien event."""
    if not rows:
        return

    tasks = sorted(set(r["task_name"] for r in rows if r["task_name"] != "TEST_CTRL"))
    if not tasks:
        return

    task_idx = {t: i for i, t in enumerate(tasks)}

    plt.figure(figsize=(14, 6))

    for r in rows:
        if r["task_name"] not in task_idx:
            continue
        y = task_idx[r["task_name"]]
        x = r["timestamp_ms"]

        # Mau theo status
        s = r["status"]
        if "PASS" in s:
            color = "seagreen"
        elif "FAIL" in s:
            color = "crimson"
        elif "WARN" in s:
            color = "orange"
        else:
            color = "steelblue"

        plt.scatter(x, y, c=color, s=15, alpha=0.6, edgecolors="none")

    plt.yticks(range(len(tasks)), tasks)
    plt.xlabel("Timestamp (ms)")
    plt.title("Timeline Events Theo Task")
    plt.grid(axis="x", linestyle="--", alpha=0.5)

    # Legend
    from matplotlib.patches import Patch
    legend = [
        Patch(color="seagreen", label="PASS"),
        Patch(color="crimson", label="FAIL"),
        Patch(color="orange", label="WARNING"),
        Patch(color="steelblue", label="KHAC"),
    ]
    plt.legend(handles=legend, loc="upper right")

    plt.tight_layout()
    plt.savefig("chart_timeline.png", dpi=100)
    plt.close()
    print("[+] Da xuat: chart_timeline.png")


def main():
    rows = read_log(LOG_FILE)
    if not rows:
        return

    print(f"[*] Doc duoc {len(rows)} events tu {LOG_FILE}")
    print()

    write_report(rows)
    chart_events_by_task(rows)
    chart_status_pie(rows)
    chart_exec_time_by_task(rows)
    chart_timeline(rows)

    print()
    print("Hoan thanh! Cac file da tao:")
    print(f"  - {REPORT_FILE}")
    print("  - chart_events.png")
    print("  - chart_status.png")
    print("  - chart_exec_time.png")
    print("  - chart_timeline.png")


if __name__ == "__main__":
    main()
