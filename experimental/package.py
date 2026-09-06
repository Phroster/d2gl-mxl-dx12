"""Package the main DX12 renderer and player guides, never a game installation."""
from pathlib import Path
import argparse,hashlib,json,subprocess,zipfile
p=argparse.ArgumentParser()
p.add_argument("--build-dir",type=Path,required=True)
args=p.parse_args()
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
 "docs/banner.svg":root/"docs/banner.svg",
 "docs/launcher-settings.png":root/"docs/launcher-settings.png",
 "docs/SETTINGS.md":root/"docs/SETTINGS.md",
 "docs/INSTALL.md":root/"docs/INSTALL.md",
 "docs/BUILD.md":root/"docs/BUILD.md",
 "docs/ARCHITECTURE.md":root/"docs/ARCHITECTURE.md",
 "docs/VALIDATION.md":root/"docs/VALIDATION.md",
 "docs/RELEASE-NOTES.md":root/"docs/RELEASE-NOTES.md",
 "docs/BINARY-VERIFICATION.json":root/"docs/BINARY-VERIFICATION.json",
 "docs/UPSTREAM.json":root/"docs/UPSTREAM.json",
 "experimental/VALIDATION.md":root/"experimental/VALIDATION.md",
 "experimental/DEPENDENCIES.json":root/"experimental/DEPENDENCIES.json",
 "experimental/checkpoints/2026-09-06.json":root/"experimental/checkpoints/2026-09-06.json",
 "LICENSE":root/"LICENSE",
 "licenses/D2GL-GPL.txt":root/"d2gl/LICENSE.md",
 "licenses/D2GL-THIRD-PARTY.txt":root/"d2gl/THIRD_PARTY_LICENSES.md",
 "licenses/D2FPS-GPL.txt":root/"d2fps/LICENSE-GPL.txt",
 "licenses/D2FPS-MIT.txt":root/"d2fps/LICENSE-MIT.txt",
 "licenses/D2FPS-APACHE.txt":root/"d2fps/LICENSE-APACHE.txt",
 "licenses/glslang.txt":root/"external/DiligentCore/ThirdParty/glslang/LICENSE.txt",
 "licenses/SPIRV-Cross.txt":root/"external/DiligentCore/ThirdParty/SPIRV-Cross/LICENSE",
 "licenses/ImGui.txt":root/"d2gl/d2gl/vendor/include/imgui/LICENSE.txt",
}
files={name:path.read_bytes() for name,path in inputs.items()}
ini=(root/"defaults/d2gl.ini").read_text(encoding="utf-8")
ini=ini.replace("; Preferred OpenGL Version (must be 3.3 or between 4.0 to 4.6).\ngl_ver_major=4\ngl_ver_minor=6",
                "; This build uses DirectX 12 automatically.")
files["d2gl.ini"]=ini.encode("utf-8")
entries=[{"name":n,"bytes":len(b),"sha256":hashlib.sha256(b).hexdigest()} for n,b in files.items()]
commit=subprocess.check_output(["git","rev-parse","HEAD"],cwd=root).decode().strip()
manifest={"product":"MXL Smooth Motion DX12","version":"1.0","source_commit":commit,
          "source_repository":"https://github.com/Phroster/mxl-smooth-motion-dx12",
          "source_branch":"main","official_d2fps_sha256":"db9de4d4d320a7b70e66fe6b4aaa0e6f1560a5300a4993cc81cf4512ab1240c1",
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
