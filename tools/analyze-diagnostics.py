"""Summarize a private MXL diagnostic session without modifying it."""
import argparse
import csv
import json
import hashlib
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


def asset_scopes(folder):
    path = Path(folder) / "assets.csv"
    if not path.exists():
        return []
    with path.open(encoding="utf-8", newline="") as source:
        rows = [r for r in csv.DictReader(source) if r.get("result") is not None]
    events = []
    for r in rows:
        event = dict(operation=r["operation"], source=r["source"], handle=int(r["handle"]),
                     session_ms=float(r["session_ms"]), duration_ms=float(r["duration_ms"]),
                     thread_id=int(r["thread_id"]), path=r["path"] or None,
                     path_status=int(r["path_status"]), requested_bytes=int(r["requested_bytes"]),
                     completed_bytes=int(r["completed_bytes"]) if r["output_valid"] == "1" else None,
                     result=int(r["result"]))
        event["end_ms"] = event["session_ms"] + event["duration_ms"]
        events.append(event)
    # Apply completed successful lifetime operations before each read starts.
    # A failed close does not release the name; failed or missing opens never
    # invent a name. Opens replace earlier lifetimes when handles are reused.
    lifecycle = sorted((e for e in events if e["operation"] in ("open", "close") and e["result"] and e["handle"]),
                       key=lambda e: e["end_ms"])
    live = {}
    cursor = 0
    for read in sorted((e for e in events if e["operation"] == "read"), key=lambda e: e["session_ms"]):
        while cursor < len(lifecycle) and lifecycle[cursor]["end_ms"] <= read["session_ms"]:
            event = lifecycle[cursor]
            if event["operation"] == "open":
                live[event["handle"]] = event
            else:
                live.pop(event["handle"], None)
            cursor += 1
        opened = live.get(read["handle"])
        if opened and opened["path_status"] in (0, 1):
            read["path"] = opened["path"]
            read["path_status"] = opened["path_status"]
    return events


def loot_work_summary(folder, frames, producers):
    groups = defaultdict(list)
    missing = 0
    for frame in frames:
        producer = producers.get(frame["frame_id"])
        if not producer or "loot_enabled" not in producer:
            missing += 1
            continue
        state = "effects_off" if producer["loot_enabled"] == "0" else (
            "effects_on_loot_visible" if int(producer["loot_targets"]) else "effects_on_no_loot")
        groups[state].append((frame, producer))

    def distribution(values):
        values = sorted(values)
        if not values:
            return None
        return {"median": statistics.median(values), "p95": values[int((len(values)-1)*.95)],
                "p99": values[int((len(values)-1)*.99)], "max": values[-1]}

    states = {}
    for state, items in groups.items():
        timings = ("producer_build_ms", "loot_effects_ms", "loot_labels_ms", "loot_names_ms")
        counts = ("loot_targets", "loot_labels", "loot_sprites", "loot_name_formats", "loot_selection_calls",
                  "loot_selection_samples", "loot_inventory_queries", "loot_unit_lookups", "loot_cache_reclaims",
                  "loot_cache_uploads", "loot_cache_hits", "loot_cache_skipped")
        entry = {"frames": len(items), "slow_frames": sum(f["slow"] == "1" for f, _ in items),
                 "frame_interval_ms": distribution(float(f["interval_ms"]) for f, _ in items if float(f["interval_ms"]) > 0),
                 "timings_ms": {name: distribution(float(p[name]) for _, p in items if name in p) for name in timings},
                 "counts_total": {name: sum(int(p.get(name, 0)) for _, p in items) for name in counts}}
        samples = sum(int(p.get("loot_selection_samples", 0)) for _, p in items)
        entry["mean_timed_pickup_sample_ms"] = sum(float(p.get("loot_pickup_sampled_ms", 0)) for _, p in items) / samples if samples else None
        states[state] = entry
    snapshot = None
    path = Path(folder) / "lootfilterconf.json"
    if path.exists():
        try:
            contents = path.read_bytes()
            profiles = json.loads(contents.decode("utf-8-sig"))
            snapshot = {"sha256": hashlib.sha256(contents).hexdigest(),
                        "active_profiles": [{"name": p["name"], "rules": len(p["rules"])} for p in profiles if p.get("active")]}
        except (ValueError, KeyError, TypeError, OSError) as error:
            snapshot = {"error": str(error)}
    return {"states": states, "frames_without_loot_metrics": missing, "saved_filter_at_launch": snapshot,
            "limits": "Groups describe the effects, not native filter activation. The saved filter snapshot does not observe later in-game menu changes. Pickup times cover sampled calls only, not total input cost. Name time is nested in label time; do not add them. Producer build includes game-thread drawing but is not an isolated measurement of Sigma filter evaluation. Same-frame correlation does not establish cause. These frame intervals are renderer completion cadence, not displayed-frame timestamps."}


def motion_summary(frames, producers, frequency):
    """Describe observations without calling a stationary character a stall."""
    pairs, fresh, steps = [], [], []
    valid = player_valid = 0
    previous = None
    for frame in sorted(frames, key=lambda f: int(f["frame_id"])):
        current = producers.get(frame["frame_id"], {})
        valid += current.get("motion_valid") == "1"
        player_valid += current.get("motion_player_valid") == "1"
        if previous and int(frame["frame_id"]) == int(previous[0]["frame_id"]) + 1:
            old = previous[1]
            if 'context_valid' in current or 'context_valid' in old:
                if current.get('context_valid') != '1' or old.get('context_valid') != '1' or current['level'] != old['level']:
                    previous = frame, current
                    continue
            if current.get("motion_valid") == old.get("motion_valid") == "1" and current["motion_game_type"] == old["motion_game_type"]:
                # DWORD counters wrap. Backwards/reset jumps are not observations.
                samples = (int(current["motion_samples"]) - int(old["motion_samples"])) & 0xffffffff
                updates = (int(current["motion_client_updates"]) - int(old["motion_client_updates"])) & 0xffffffff
                if samples < 10000 and updates < 10000:
                    pairs.append((samples, updates))
                    if samples:
                        fresh.append(current)
            if current.get("motion_player_valid") == old.get("motion_player_valid") == "1" and all(
                    current[name] == old[name] for name in ("motion_player_id", "motion_panels")):
                def delta(name):
                    return ((int(current[name]) - int(old[name]) + 0x80000000) & 0xffffffff) - 0x80000000
                steps.append({"path_distance": (delta("motion_player_x") ** 2 + delta("motion_player_y") ** 2) ** .5 / 65536,
                              "camera_distance": (delta("motion_camera_x") ** 2 + delta("motion_camera_y") ** 2) ** .5})
        previous = frame, current

    def distribution(values):
        values = sorted(values)
        return {"median": statistics.median(values), "p99": values[int((len(values)-1)*.99)], "max": values[-1]} if values else None

    def signed_ticks(row, name):
        value = int(row[name])
        return value - (1 << 64) if value >= (1 << 63) else value

    return {"available": bool(valid or player_valid), "valid_clamp_frames": valid, "observed_player_frames": player_valid,
            "adjacent_counter_pairs": len(pairs), "fresh_clamp_rows": len(fresh),
            "unchanged_clamp_counter_pairs": sum(samples == 0 for samples, _ in pairs),
            "client_update_steps": dict(Counter(updates for _, updates in pairs)),
            "fresh_clamp_elapsed_ms": distribution(signed_ticks(p, "motion_elapsed_ticks") * 1000 / frequency for p in fresh) if frequency else None,
            "fresh_clamp_output_ms": distribution(signed_ticks(p, "motion_clamped_ticks") * 1000 / frequency for p in fresh) if frequency else None,
            "fresh_rows_limited": sum(p["motion_elapsed_ticks"] != p["motion_clamped_ticks"] for p in fresh),
            "negative_phase_rows": sum(signed_ticks(p, "motion_elapsed_ticks") < 0 for p in fresh),
            "adjacent_player_pairs": len(steps), "unchanged_player_pairs": sum(s["path_distance"] == 0 for s in steps),
            "player_step_tiles": distribution(s["path_distance"] for s in steps),
            "camera_step_pixels": distribution(s["camera_distance"] for s in steps),
            "limits": "Only adjacent recorded gameplay frames are paired. Counter resets and game-type changes are excluded; unchanged clamp counters contain stale values. Schema 1 did not observe negative-time paths; schema 2 observes both signs. Player pairs require the same player and panel state; area transitions can still jump. Stationary coordinates do not establish stutter without a known continuous-movement segment. These are draw-time observations, not displayed-frame measurements."}


def comprehensive_summary(rows, frames, producers):
    def distribution(values):
        values = sorted(values)
        return {"median": statistics.median(values), "p95": values[int((len(values)-1)*.95)],
                "p99": values[int((len(values)-1)*.99)], "max": values[-1]} if values else None

    groups = defaultdict(list)
    for frame in frames:
        producer = producers.get(frame['frame_id'], {})
        if producer.get('context_valid') == '1':
            key = (int(producer['level']), int(producer['player_mode']), int(producer['motion_panels']), int(producer['loot_targets']) > 0)
            groups[key].append((frame, producer))
    contexts = []
    for (level, mode, panels, loot), items in sorted(groups.items()):
        contexts.append(dict(level=level, player_mode=mode, panels=panels, loot_visible=loot, frames=len(items),
            frame_interval_ms=distribution(float(f['interval_ms']) for f, _ in items),
            producer_timings_ms={key: distribution(float(p[key]) for _, p in items if key in p)
                for key in ('producer_build_ms', 'game_world_ms', 'game_ui_ms', 'game_map_ms', 'loot_effects_ms', 'loot_labels_ms', 'probe_ms')},
            max_world_hook_visits=max((int(p.get('world_units', 0)) for _, p in items), default=0)))
    probed = [f for f in frames if f.get('present_probed') == '1']
    successes = [f for f in probed if int(f['present_stats_result']) < (1 << 31)]
    previous = None
    refresh_steps = Counter()
    repeats = 0
    for frame in sorted(frames, key=lambda f: int(f['frame_id'])):
        if frame.get('present_probed') != '1' or int(frame['present_stats_result']) >= (1 << 31):
            previous = None
            continue
        if previous and int(frame['frame_id']) == int(previous['frame_id'])+1:
            count = (int(frame['present_stats_count'])-int(previous['present_stats_count'])) & 0xffffffff
            refresh = (int(frame['present_refresh'])-int(previous['present_refresh'])) & 0xffffffff
            if count == 1 and refresh < 10000:
                refresh_steps[refresh] += 1
            elif count == 0:
                repeats += 1
        previous = frame
    actions = [r for r in rows if r['type'] == 'input_event']
    processes = [r for r in rows if r['type'] == 'process']
    logger_cpu = None
    if len(processes) >= 2:
        first, last = processes[0], processes[-1]
        elapsed = float(last['session_ms'])-float(first['session_ms'])
        if first.get('logger_cpu_valid') == last.get('logger_cpu_valid') == '1' and elapsed > 0:
            used = int(last['logger_cpu'])-int(first['logger_cpu'])
            if used >= 0:
                logger_cpu = used / 10000 / elapsed * 100
    return {'available': bool(contexts or probed or processes), 'contexts': contexts,
            'presentation': {'queries': len(probed), 'successful_queries': len(successes),
                'results': dict(Counter(hex(int(f['present_stats_result'])) for f in probed)),
                'refresh_steps_for_consecutive_present_ids': dict(refresh_steps), 'repeated_statistics': repeats,
                'query_cost_ms': distribution(float(f['present_probe_ms']) for f in probed)},
            'actions': {'counts': dict(Counter(r['detail'] for r in actions)),
                'handler_ms': distribution(float(r['duration_ms']) for r in actions),
                'slowest': [{'category': r['detail'], 'session_ms': float(r['session_ms']), 'handler_ms': float(r['duration_ms'])}
                            for r in sorted(actions, key=lambda r: float(r['duration_ms']), reverse=True)[:20]]},
            'process': {'samples': len(processes), 'logger_cpu_percent_of_one_core': logger_cpu,
                'max_private_bytes': max((int(r['private_bytes']) for r in processes if int(r['process_valid']) & 4), default=None),
                'max_working_set': max((int(r['working_set']) for r in processes if int(r['process_valid']) & 4), default=None)},
            'queue_peak': max((int(r['queue_peak']) for r in rows if r['type'] == 'logger' and 'queue_peak' in r), default=None),
            'limits': 'Context requires an observed local player and is joined by frame ID. Area/action/panel changes form separate groups. Stage timings include nested effects and probe work. Successful DXGI statistics can repeat or refer to an older present; refresh counters are not per-frame scanout timestamps and may be unreliable with multiple monitors. Failed, skipped and disjoint queries are not zero frame time. Process counters include diagnostics. Logger CPU is averaged over the sampled interval, not peak overhead. Missing rows and retention overwrites limit conclusions.'}


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
    assets = asset_scopes(folder)
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
        item["loot"] = {k: float(v) if k.endswith("_ms") else int(v) for k, v in p.items() if k.startswith("loot_")} if p else None
        item["motion"] = {k: int(v) for k, v in p.items() if k.startswith("motion_")} if p else None
        item["overlapping_sound_calls"] = [{"operation": a["detail"], "ms": float(a["duration_ms"]), "thread": int(a["thread_id"])} for a in overlap[:20]]
        item["overlapping_asset_calls"] = sorted((a for a in assets if a["session_ms"] < end and a["end_ms"] > start),
                                                key=lambda a: a["duration_ms"], reverse=True)[:20]
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
    reads = [a for a in assets if a["operation"] == "read"]
    file_reads = defaultdict(list)
    for event in reads:
        if event["path"] is not None:
            file_reads[event["path"]].append(event)
    asset_summary = {"records": len(assets), "operations": dict(Counter(a["operation"] for a in assets)),
        "unmatched_reads": sum(a["path"] is None for a in reads),
        "slowest_reads": sorted(reads, key=lambda a: a["duration_ms"], reverse=True)[:20],
        "files_by_total_read_ms": sorted((dict(path=path, calls=len(items),
             total_read_ms=sum(a["duration_ms"] for a in items), max_read_ms=max(a["duration_ms"] for a in items),
             requested_bytes=sum(a["requested_bytes"] for a in items),
             valid_completed_bytes=sum(a["completed_bytes"] or 0 for a in items)) for path, items in file_reads.items()),
             key=lambda a: a["total_read_ms"], reverse=True)[:20],
        "limits": "Only the guarded D2CMP/D2Sound Fog imports are measured. Missing open/close records or handle reuse can limit filename attribution. A truncated path is flagged. Read wall time includes Storm work and cached I/O; it is not a physical-disk measurement and excludes later sprite decoding/mixing. Async reads time submission only. Overlap is correlation, not cause."}
    metadata = {}
    for filename in ("session.txt", "status.txt"):
        path = Path(folder) / filename
        if path.exists():
            metadata[filename] = dict(line.split("=", 1) for line in path.read_text(encoding="utf-8-sig").splitlines() if "=" in line)
    return {"session": str(folder), "metadata": metadata,
            "retained_event_range_ms": [float(rows[0]["session_ms"]), float(rows[-1]["session_ms"])] if rows else None,
            "focused_gameplay_frames": len(frames), "slow_frames": len(slow),
            "median_frame_ms": statistics.median(float(r["interval_ms"]) for r in frames) if frames else None,
            "dropped_records": max((int(r["value"]) for r in rows if r["type"] == "logger"), default=0),
            "audio_calls": dict(audio_totals), "audio_long_or_failed_calls": len(audio), "notes": notes,
            "asset_io": asset_summary,
            "loot_work": loot_work_summary(folder, frames, producer),
            "comprehensive": comprehensive_summary(rows, frames, producer),
            "motion": motion_summary(frames, producer, int(metadata.get("session.txt", {}).get("qpc_frequency", 0))),
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
