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
        item["overlapping_sound_calls"] = [{"operation": a["detail"], "ms": float(a["duration_ms"]), "thread": int(a["thread_id"])} for a in overlap[:20]]
        worst.append(item)
    notes = [{"event": r["detail"], "value": int(r["value"])} for r in rows if r["type"] == "note" and not r["detail"].startswith("shader_compile")]
    return {"session": str(folder), "focused_gameplay_frames": len(frames), "slow_frames": len(slow),
            "median_frame_ms": statistics.median(float(r["interval_ms"]) for r in frames) if frames else None,
            "dropped_records": max((int(r["value"]) for r in rows if r["type"] == "logger"), default=0),
            "audio_calls": dict(audio_totals), "audio_long_or_failed_calls": len(audio), "notes": notes,
            "worst_frames": worst,
            "limits": "GPU time excludes ReShade's separate submissions. Sound overlap is correlation, not proof of cause. Null GPU/producer timing means unavailable, not zero."}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("session", type=Path)
    args = parser.parse_args()
    print(json.dumps(analyze(args.session), indent=2))
