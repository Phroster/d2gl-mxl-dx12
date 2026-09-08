"""Exercise file-handle reuse and missing/failed lifetime records offline."""
import csv
import importlib.util
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("diagnostics_analyzer", root / "tools/analyze-diagnostics.py")
analyzer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analyzer)
output = Path(sys.argv[1]).resolve()
output.mkdir(parents=True, exist_ok=True)
header = ["operation", "source", "handle", "session_ms", "thread_id", "duration_ms", "path", "path_status",
          "requested_bytes", "completed_bytes", "output_valid", "result"]
events = []
def add(op, at, handle=7, path="", status=3, result=1, duration=.1, valid=1):
    events.append([op,"D2CMP",handle,at,123,duration,path,status,17 if op=="read" else 0,17 if op=="read" else 0,valid,result])
add("open",10,path='first,"cold.dcc',status=0,duration=2)
add("read",11)  # Open has not returned yet.
add("read",13)
add("close",14,result=0)
add("read",15)
add("close",16)
add("read",17)
add("open",18,path="second.wav",status=0)
add("read",19)
add("open",20,handle=0,path="failed.wav",status=0,result=0,valid=0)
add("read",21,result=0,valid=0)
add("open",22,path="",status=2)
add("read",23)
with (output / "assets.csv").open("w",newline="") as target:
    writer=csv.writer(target);writer.writerow(header);writer.writerows(events)
rows=analyzer.asset_scopes(output)
reads=[r for r in rows if r["operation"]=="read"]
assert [r["path"] for r in reads] == [None,'first,"cold.dcc','first,"cold.dcc',None,"second.wav","second.wav",None]
assert reads[-2]["completed_bytes"] is None
result=analyzer.analyze(output)
assert result["asset_io"]["unmatched_reads"]==3
assert result["asset_io"]["operations"]=={"open":4,"read":7,"close":2}
assert result["focused_gameplay_frames"]==0
print("PASS: quoted paths, open completion timing, failed close/open, reused handles, missing names, invalid byte outputs and asset-only reports.")
