"""Package the main DX12 renderer and player guides, never a game installation."""
from pathlib import Path
import argparse,configparser,hashlib,json,subprocess,zipfile
p=argparse.ArgumentParser()
p.add_argument("--build-dir",type=Path,required=True)
args=p.parse_args()
dependencies=json.loads((args.build_dir/"mxl-build-dependencies.json").read_text(encoding="utf-8"))
root=Path(__file__).resolve().parents[1]
name="mxl-smooth-motion-dx12-1.0"
out=root/"dist"/name
out.mkdir(parents=True,exist_ok=True)
inputs={
 "glide3x.dll":args.build_dir/"Release/glide3x.dll",
 "ddraw.dll":args.build_dir/"Release/ddraw.dll",
 "d2gl.mpq":root/"d2gl/d2gl.mpq",
 "d2fps.ini":root/"defaults/d2fps.ini",
 "README.md":root/"README.md",
 "START-HERE.txt":root/"START-HERE.txt",
 "docs/banner-dx12-v1.0.svg":root/"docs/banner-dx12-v1.0.svg",
 "mxl-diagnostics.ini":root/"mxl-diagnostics.ini",
 "docs/launcher-settings.png":root/"docs/launcher-settings.png",
 "docs/SETTINGS.md":root/"docs/SETTINGS.md",
 "docs/INSTALL.md":root/"docs/INSTALL.md",
 "docs/RELEASE-NOTES.md":root/"docs/RELEASE-NOTES.md",
 "LICENSE":root/"LICENSE",
 "licenses/D2GL-GPL.txt":root/"d2gl/LICENSE.md",
 "licenses/D2GL-THIRD-PARTY.txt":root/"d2gl/THIRD_PARTY_LICENSES.md",
 "licenses/D2FPS-GPL.txt":root/"licenses/D2FPS-GPL.txt",
 "licenses/D2FPS-MIT.txt":root/"licenses/D2FPS-MIT.txt",
 "licenses/D2FPS-APACHE.txt":root/"licenses/D2FPS-APACHE.txt",
 "licenses/glslang.txt":Path(dependencies["glslang"])/"LICENSE.txt",
 "licenses/SPIRV-Cross.txt":Path(dependencies["spirv_cross"])/"LICENSE",
 "licenses/ImGui.txt":root/"d2gl/d2gl/vendor/include/imgui/LICENSE.txt",
}
files={name:path.read_bytes() for name,path in inputs.items()}
# Source-only footer links remain valid when the README is opened from the ZIP.
readme=files["README.md"].decode("utf-8")
for page in ("BUILD.md","ARCHITECTURE.md","UPSTREAM.json"):
    readme=readme.replace("(docs/"+page+")","(https://github.com/Phroster/d2gl-mxl-dx12/blob/master/docs/"+page+")")
files["README.md"]=readme.encode("utf-8")
diagnostics=configparser.ConfigParser()
diagnostics.read_string(files["mxl-diagnostics.ini"].decode("utf-8"))
assert diagnostics.getint("Diagnostics","enabled")==0, "Release recording must default to off"
ini=(root/"defaults/d2gl.ini").read_text(encoding="utf-8")
ini=ini.replace("; Preferred OpenGL Version (must be 3.3 or between 4.0 to 4.6).\ngl_ver_major=4\ngl_ver_minor=6",
                "; This build uses DirectX 12 automatically.")
files["d2gl.ini"]=ini.encode("utf-8")
entries=[{"name":n,"bytes":len(b),"sha256":hashlib.sha256(b).hexdigest()} for n,b in files.items()]
commit=subprocess.check_output(["git","rev-parse","HEAD"],cwd=root).decode().strip()
manifest={"product":"MXL Smooth Motion DX12","version":"1.0","source_commit":commit,
          "performance_recording_default":False,"automatic_act_reveal":True,
          "source_repository":"https://github.com/Phroster/d2gl-mxl-dx12",
          "source_branch":"master","official_d2fps_sha256":"db9de4d4d320a7b70e66fe6b4aaa0e6f1560a5300a4993cc81cf4512ab1240c1",
          "files":entries}
files["manifest.json"]=(json.dumps(manifest,indent=2)+"\n").encode("utf-8")
for n,b in files.items():
    target=out/n;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(b)
archive=out.parent/(name+".zip")
with zipfile.ZipFile(archive,"w",compression=zipfile.ZIP_DEFLATED) as z:
    for n in files:z.write(out/n,n)
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    assert "d2fps.dll" not in z.namelist()
    for entry in entries:assert hashlib.sha256(z.read(entry["name"])).hexdigest()==entry["sha256"]
print(json.dumps({"zip":str(archive),"bytes":archive.stat().st_size,"sha256":hashlib.sha256(archive.read_bytes()).hexdigest()},indent=2))
