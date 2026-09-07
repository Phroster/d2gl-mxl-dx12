"""Summarize a private MXL diagnostic session without modifying it."""
import argparse
import csv
import json
import statistics
from collections import Counter, defaultdict
from decimal import Decimal
from pathlib import Path


REVEAL_DEEP_PHASES = (
    "preset_generation", "preset_build_area", "preset_room_prepare", "dt1_load",
    "room_tile_grid", "level_lookup", "automap_layer",
)
REVEAL_TICKS_PER_MS = 1_000_000  # Preserve the CSV's six decimal places exactly.


def reveal_scopes(events):
    scopes = []
    for position, row in enumerate(events):
        start = int(Decimal(row["session_ms"]) * REVEAL_TICKS_PER_MS)
        duration = int(Decimal(row["duration_ms"]) * REVEAL_TICKS_PER_MS)
        scopes.append({"position": position, "phase": row["phase"], "thread_id": int(row["thread_id"]),
                       "level": int(row["level"]), "start": start, "end": start + duration})
    return scopes


def interval_union_ticks(intervals):
    """Merge nested, overlapping and touching intervals without adding their costs."""
    total = 0
    start = end = None
    for left, right in sorted(intervals):
        if right <= left:
            continue
        if end is None:
            start, end = left, right
        elif left <= end:
            end = max(end, right)
        else:
            total += end - start
            start, end = left, right
    return total + (end - start if end is not None else 0)


def reveal_scope_coverage(parent, scopes):
    """Account for recorded descendants, excluding containing/other-thread scopes."""
    children = []
    for scope in scopes:
        if scope is parent or scope["thread_id"] != parent["thread_id"]:
            continue
        # Independent rounding of start and duration can differ by one CSV tick.
        if scope["start"] < parent["start"] - 1 or scope["end"] > parent["end"] + 1:
            continue
        # Same-thread records are emitted at scope exit. This also disambiguates
        # recursive scopes whose timestamps become identical after rounding.
        if scope["position"] >= parent["position"]:
            continue
        children.append(scope)

    def interval(scope):
        return max(parent["start"], scope["start"]), min(parent["end"], scope["end"])

    phases = defaultdict(list)
    for child in children:
        phases[child["phase"]].append(child)
    child_union = interval_union_ticks(interval(child) for child in children)
    duration = parent["end"] - parent["start"]
    return {"level": parent["level"], "phase": parent["phase"],
            "session_ms": parent["start"] / REVEAL_TICKS_PER_MS, "thread_id": parent["thread_id"],
            "ms": duration / REVEAL_TICKS_PER_MS, "child_calls": len(children),
            "child_union_ms": child_union / REVEAL_TICKS_PER_MS,
            "uninstrumented_ms": (duration - child_union) / REVEAL_TICKS_PER_MS,
            "child_phase_stats": {
                name: {"calls": len(items),
                       "total_ms": sum(child["end"] - child["start"] for child in items) / REVEAL_TICKS_PER_MS,
                       "union_ms": interval_union_ticks(interval(child) for child in items) / REVEAL_TICKS_PER_MS}
                for name, items in sorted(phases.items())}}


def analyze(folder):
    rows = []
    for path in sorted(Path(folder).glob("events-*.csv")):
        with path.open(encoding="utf-8", newline="") as source:
            for row in csv.DictReader(source):
                if row.get("value") is None:  # Writer may be appending its final line.
                    continue
                rows.append(row)
    rows.sort(key=lambda row: float(row["session_ms"]))
    frames = [r for r in rows if r["type"] == "frame" and r["focused"] == "1" and r["game_screen"] == "1"]
    slow = [r for r in frames if r["slow"] == "1"]
    band = [r for r in frames if 1000 / 85 <= float(r["interval_ms"]) <= 1000 / 75]
    gpu = defaultdict(float)
    producer = {}
    audio = [r for r in rows if r["type"] == "audio"]
    audio_totals = Counter()
    for r in rows:
        if r["type"] == "gpu":
            gpu[r["frame_id"]] += float(r["duration_ms"])
        elif r["type"] == "producer":
            producer[r["frame_id"]] = r
        elif r["type"] == "audio_summary":
            audio_totals[r["detail"]] += int(r["draws"])
    worst = []
    for r in sorted(slow, key=lambda r: float(r["interval_ms"]), reverse=True)[:20]:
        end = float(r["session_ms"]) + float(r["duration_ms"])
        start = end - float(r["interval_ms"])
        overlap = [a for a in audio if float(a["session_ms"]) < end and float(a["session_ms"]) + float(a["duration_ms"]) > start]
        item = {k: float(r[k]) for k in ("session_ms", "interval_ms", "render_ms", "input_wait_ms", "gpu_fence_wait_ms", "present_call_ms", "latency_wait_ms", "bindings_ms", "index_scan_ms", "pipeline_ms", "allocation_ms")}
        item.update(frame_id=int(r["frame_id"]), gpu_own_ms=gpu.get(r["frame_id"]), draws=int(r["draws"]),
                    texture_bytes=int(r["texture_bytes"]), buffer_bytes=int(r["buffer_bytes"]), new_pipelines=int(r["new_pipelines"]))
        p = producer.get(r["frame_id"])
        item["producer_wait_ms"] = float(p["duration_ms"]) if p else None
        item["producer_build_ms"] = float(p["producer_build_ms"]) if p and "producer_build_ms" in p else None
        item["overlapping_sound_calls"] = [{"operation": a["detail"], "ms": float(a["duration_ms"]), "thread": int(a["thread_id"])} for a in overlap[:20]]
        worst.append(item)
    notes = [{"event": r["detail"], "session_ms": float(r["session_ms"]), "value": int(r["value"])} for r in rows if r["type"] == "note" and not r["detail"].startswith("shader_compile")]
    band_summary = {"frames": len(band)}
    if band:
        for name in ("interval_ms", "render_ms", "upload_ms", "allocation_ms", "input_wait_ms", "buffer_bytes", "spill_bytes", "draws"):
            band_summary["median_" + name] = statistics.median(float(r[name]) for r in band)
        matched = [gpu[r["frame_id"]] for r in band if r["frame_id"] in gpu]
        band_summary["median_gpu_own_ms"] = statistics.median(matched) if matched else None
    input_profiles = []
    input_path = Path(folder) / "input.csv"
    if input_path.exists():
        with input_path.open(encoding="utf-8", newline="") as source:
            for r in csv.DictReader(source):
                if r.get("procedure_offset") is None:
                    continue
                profile = {"input_id": int(r["input_id"]), "session_ms": float(r["session_ms"]), "thread_id": int(r["thread_id"]),
                           "procedure_module": r["procedure_module"], "procedure_offset": hex(int(r["procedure_offset"])), "valid_flags": int(r["valid_flags"])}
                for name in ("wall_ms", "user_cpu_ms", "kernel_cpu_ms", "wall_minus_cpu_ms"):
                    value = float(r[name]); profile[name] = value if value >= 0 else None
                for name in ("thread_cycles", "process_read_bytes", "process_read_ops", "process_write_bytes", "process_write_ops", "process_other_bytes", "process_other_ops", "process_page_faults"):
                    value = int(r[name]); profile[name] = value if value >= 0 else None
                input_profiles.append(profile)
    reveal_traces = []
    reveal_path = Path(folder) / "reveal.csv"
    if reveal_path.exists():
        grouped = defaultdict(list)
        with reveal_path.open(encoding="utf-8", newline="") as source:
            for r in csv.DictReader(source):
                if r.get("resident_before") is not None:
                    grouped[int(r["trace_id"])].append(r)
        for trace_id, events in grouped.items():
            phases = defaultdict(list)
            for r in events:
                phases[r["phase"]].append(float(r["duration_ms"]))
            stats = {}
            for name, durations in phases.items():
                values = sorted(durations)
                stats[name] = {"calls": len(values), "total_ms": sum(values), "median_ms": statistics.median(values),
                               "p95_ms": values[int((len(values) - 1) * .95)], "max_ms": values[-1]}
            rooms = sorted((r for r in events if r["phase"] == "room"), key=lambda r: float(r["duration_ms"]), reverse=True)
            generated = sorted((r for r in events if r["phase"] == "level_generation"), key=lambda r: float(r["duration_ms"]), reverse=True)
            scopes = reveal_scopes(events)
            generation_scopes = sorted((s for s in scopes if s["phase"] == "level_generation"),
                                       key=lambda s: s["end"] - s["start"], reverse=True)
            reveal_traces.append({"trace_id": trace_id, "act": int(events[0]["act"]), "phase_stats": stats,
                "heaviest_rooms": [{"level": int(r["level"]), "x": int(r["room_x"]), "y": int(r["room_y"]),
                                    "ms": float(r["duration_ms"]), "resident_before": r["resident_before"] == "1"} for r in rooms[:15]],
                "heaviest_level_generation": [{"level": int(r["level"]), "ms": float(r["duration_ms"])} for r in generated[:15]],
                "root_coverage": [reveal_scope_coverage(s, scopes) for s in scopes if s["phase"] == "act"],
                "generation_phase_breakdown": [reveal_scope_coverage(s, scopes) for s in generation_scopes[:15]],
                "deep_probe_phases_observed": [name for name in REVEAL_DEEP_PHASES if name in phases],
                "deep_probe_phases_not_observed": [name for name in REVEAL_DEEP_PHASES if name not in phases]})
    return {"session": str(folder), "focused_gameplay_frames": len(frames), "slow_frames": len(slow),
            "median_frame_ms": statistics.median(float(r["interval_ms"]) for r in frames) if frames else None,
            "dropped_records": max((int(r["value"]) for r in rows if r["type"] == "logger"), default=0),
            "audio_calls": dict(audio_totals), "audio_long_or_failed_calls": len(audio), "notes": notes,
            "fps_75_to_85": band_summary,
            "T_profiles": input_profiles,
            "reveal_traces": reveal_traces,
            "worst_frames": worst,
            "limits": "GPU time excludes ReShade's separate submissions. Sound overlap is correlation, not proof of cause. Null means unavailable, not zero. T CPU accounting has finite granularity; wall-minus-CPU includes waits and descheduling. I/O/fault counters are process-wide, include cached reads/soft faults, and do not measure physical disk traffic. The procedure module identifies the entrypoint. Reveal scopes are nested: phase total_ms values are inclusive and are not additive, including recursive calls of the same phase. Reveal child_union_ms merges all recorded same-thread descendant intervals within the parent; per-phase union_ms values can still overlap each other. uninstrumented_ms is parent time without recorded child coverage, including unhooked work, instrumentation overhead and missing/dropped records; it is not exclusive native execution cost. dropped_records is session-wide and cannot identify which reveal scopes were lost. A deep probe phase listed as not observed does not prove zero work or that its hook was installed. Incomplete/live captures may lack parent or child records."}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("session", type=Path)
    args = parser.parse_args()
    print(json.dumps(analyze(args.session), indent=2))
