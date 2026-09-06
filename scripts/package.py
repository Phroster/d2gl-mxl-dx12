"""Package the built wrappers and their matching assets; never install into a game."""
from pathlib import Path
import hashlib
import json
import shutil
import struct
import subprocess
import zipfile

root = Path(__file__).resolve().parents[1]
package = root / "dist" / "d2gl-d2fps-mxl-1.0"
package.mkdir(parents=True, exist_ok=True)
files = {
    "glide3x.dll": root / "build/bin/glide3x.dll",
    "ddraw.dll": root / "build/bin/ddraw.dll",
    "d2gl.mpq": root / "d2gl/d2gl.mpq",
    "d2gl.ini": root / "defaults/d2gl.ini",
    "d2fps.ini": root / "defaults/d2fps.ini",
    "README.md": root / "README.md",
    "docs/SETTINGS.md": root / "docs/SETTINGS.md",
    "docs/ARCHITECTURE.md": root / "docs/ARCHITECTURE.md",
    "docs/VALIDATION.md": root / "docs/VALIDATION.md",
    "docs/NATIVE-TESTS.txt": root / "docs/NATIVE-TESTS.txt",
    "docs/D2FPS-SOURCE-TESTS.txt": root / "docs/D2FPS-SOURCE-TESTS.txt",
    "docs/INSTALLER-TESTS.txt": root / "docs/INSTALLER-TESTS.txt",
    "docs/BINARY-VERIFICATION.json": root / "docs/BINARY-VERIFICATION.json",
    "LICENSE": root / "LICENSE",
    "scripts/install.ps1": root / "scripts/install.ps1",
    "scripts/restore.ps1": root / "scripts/restore.ps1",
    "licenses/D2GL-GPL.txt": root / "d2gl/LICENSE.md",
    "licenses/D2GL-THIRD-PARTY.txt": root / "d2gl/THIRD_PARTY_LICENSES.md",
    "licenses/D2FPS-GPL.txt": root / "d2fps/LICENSE-GPL.txt",
    "licenses/D2FPS-MIT.txt": root / "d2fps/LICENSE-MIT.txt",
    "licenses/D2FPS-APACHE.txt": root / "d2fps/LICENSE-APACHE.txt",
}
entries = []
for name, source in files.items():
    data = source.read_bytes()
    if name.endswith(".dll"):
        assert data[:2] == b"MZ", name
        pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
        assert data[pe_offset:pe_offset + 4] == b"PE\0\0", name
        assert struct.unpack_from("<H", data, pe_offset + 4)[0] == 0x14C, name
    target = package / name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)
    entries.append({"name": name, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
assert next(x["sha256"] for x in entries if x["name"] == "d2gl.mpq") == "f6c85e6f0b77df3524dd7f4aafa66fd81ddf505907e363b531a6d47b0a7d201a"
revision = subprocess.run(["git", "rev-parse", "HEAD"], cwd=root, text=True, capture_output=True)
manifest = {
    "version": "1.0",
    "source_repository": "https://github.com/Phroster/d2gl-d2fps-mxl",
    "source_commit": revision.stdout.strip() if revision.returncode == 0 else "uncommitted-local-build",
    "official_d2fps_sha256": "db9de4d4d320a7b70e66fe6b4aaa0e6f1560a5300a4993cc81cf4512ab1240c1",
    "files": entries,
}
(package / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
archive_path = package.parent / (package.name + ".zip")
with zipfile.ZipFile(archive_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
    for name in [*files, "manifest.json"]:
        archive.write(package / name, name)
with zipfile.ZipFile(archive_path) as archive:
    assert archive.testzip() is None
    assert "d2fps.dll" not in archive.namelist()
    for entry in entries:
        assert hashlib.sha256(archive.read(entry["name"])).hexdigest() == entry["sha256"]
print(json.dumps({"zip": str(archive_path), "bytes": archive_path.stat().st_size,
                  "sha256": hashlib.sha256(archive_path.read_bytes()).hexdigest()}, indent=2))
