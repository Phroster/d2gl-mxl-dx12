"""Summarize a private MXL diagnostic session without modifying it."""
import argparse
import csv
import json
import statistics
from collections import Counter, defaultdict
from pathlib import Path


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
    return {"session": str(folder), "focused_gameplay_frames": len(frames), "slow_frames": len(slow),
            "median_frame_ms": statistics.median(float(r["interval_ms"]) for r in frames) if frames else None,
            "dropped_records": max((int(r["value"]) for r in rows if r["type"] == "logger"), default=0),
            "audio_calls": dict(audio_totals), "audio_long_or_failed_calls": len(audio), "notes": notes,
            "fps_75_to_85": band_summary,
            "T_profiles": input_profiles,
            "worst_frames": worst,
            "limits": "GPU time excludes ReShade's separate submissions. Sound overlap is correlation, not proof of cause. Null means unavailable, not zero. T CPU accounting has finite granularity; wall-minus-CPU includes waits and descheduling. I/O/fault counters are process-wide, include cached reads/soft faults, and do not measure physical disk traffic. The procedure module identifies the entrypoint, not a sampled inner hotspot."}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("session", type=Path)
    args = parser.parse_args()
    print(json.dumps(analyze(args.session), indent=2))
