"""Create the player ZIP from an explicit list of installation files and notices."""
import argparse
import configparser
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile


ROOT = Path(__file__).resolve().parents[1]
NAME = "mxl-smooth-motion-dx12-1.12"
REPOSITORY = "https://github.com/Phroster/d2gl-mxl-dx12"
PLAYER_FILES = frozenset({
    "glide3x.dll", "ddraw.dll", "d2gl.mpq", "d2gl.ini", "d2fps.ini",
    "mxl-diagnostics.ini", "mxl-native-loot.ini", "LICENSES.txt",
})


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def license_notices(commit, dependencies):
    sections = [
        "MXL Smooth Motion DX12 1.12 - copyright, licenses and source\n\n"
        "The modified D2GL code is free software under GNU GPL version 3\n"
        "or (at your option) any later version. It comes WITHOUT ANY WARRANTY.\n"
        "Original D2GL: Copyright (C) 2023 Bayaraa.\n"
        "D2DX motion prediction: Bolrog and contributors.\n"
        "Median XL adaptations: Pooquer, GavinK88 and contributors.\n"
        "DX12 rendering and integration changes: Phroster, September 2026.\n\n"
        f"Exact source revision: {commit}\n"
        f"Source and build instructions: {REPOSITORY}/tree/{commit}\n"
        f"Source download: {REPOSITORY}/archive/{commit}.zip\n"
        f"Licensing and dependency sources: {REPOSITORY}/blob/{commit}/docs/LICENSING.md\n\n"
        "D2FPS is provided by the installed Median XL game and is not included\n"
        "in this ZIP. Its historical license texts remain in the source repository.\n"
        "Third-party components retain their own copyright and license terms below.\n",
        "GNU GENERAL PUBLIC LICENSE\n\n" + (ROOT / "LICENSE").read_text(encoding="utf-8"),
    ]
    inventory = json.loads((ROOT / "licenses/player-notices.json").read_text(encoding="utf-8"))
    for item in inventory:
        path = ROOT / item["file"]
        contents = path.read_text(encoding="utf-8")
        if "build_dependency" in item:
            original = Path(dependencies[item["build_dependency"]]) / item["dependency_file"]
            # Git may change line endings on checkout; compare the complete text.
            if original.read_text(encoding="utf-8") != contents:
                raise ValueError(f"License differs from the build dependency: {original}")
        sections.append(item["title"] + "\n" + item["source"] + "\n\n" + contents)
    return ("\n\n" + "=" * 72 + "\n\n").join(sections).encode("utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, default=ROOT / "dist")
    args = parser.parse_args()
    dependencies = json.loads((args.build_dir / "mxl-build-dependencies.json").read_text(encoding="utf-8"))
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    inputs = {
        "glide3x.dll": args.build_dir / "Release/glide3x.dll",
        "ddraw.dll": args.build_dir / "Release/ddraw.dll",
        "d2gl.mpq": ROOT / "d2gl/d2gl.mpq",
        "d2fps.ini": ROOT / "defaults/d2fps.ini",
        "mxl-diagnostics.ini": ROOT / "mxl-diagnostics.ini",
        "mxl-native-loot.ini": ROOT / "mxl-native-loot.ini",
    }
    files = {name: path.read_bytes() for name, path in inputs.items()}
    diagnostics = configparser.ConfigParser()
    diagnostics.read_string(files["mxl-diagnostics.ini"].decode("utf-8"))
    if diagnostics.getint("Diagnostics", "enabled") != 0:
        raise ValueError("Release recording must default to off")
    loot = configparser.ConfigParser()
    loot.read_string(files["mxl-native-loot.ini"].decode("utf-8"))
    if loot.getint("NativeLoot", "Enabled") != 1:
        raise ValueError("Release loot effects must default to on")
    if loot.getint("NativeLoot", "ClickEffects") != 1:
        raise ValueError("Release effect pickup must default to on")
    ini = (ROOT / "defaults/d2gl.ini").read_text(encoding="utf-8")
    ini = ini.replace(
        "; Preferred OpenGL Version (must be 3.3 or between 4.0 to 4.6).\ngl_ver_major=4\ngl_ver_minor=6",
        "; This build uses DirectX 12 automatically.",
    )
    files["d2gl.ini"] = ini.encode("utf-8")
    files["LICENSES.txt"] = license_notices(commit, dependencies)
    if set(files) != PLAYER_FILES:
        raise ValueError("Unexpected player package contents")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    archive = args.output_dir / (NAME + ".zip")
    temporary = archive.with_suffix(".zip.tmp")
    # Write bytes directly: old staging folders can never leak files into a release.
    with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED) as output:
        for name in sorted(files):
            output.writestr(name, files[name])
    with zipfile.ZipFile(temporary) as output:
        if set(output.namelist()) != PLAYER_FILES or len(output.namelist()) != len(PLAYER_FILES):
            raise ValueError("Unexpected ZIP entries")
        if output.testzip() is not None:
            raise ValueError("ZIP integrity check failed")
        for name, contents in files.items():
            if sha256(output.read(name)) != sha256(contents):
                raise ValueError(f"ZIP data differs: {name}")
    temporary.replace(archive)
    digest = sha256(archive.read_bytes())
    manifest = {
        "product": "MXL Smooth Motion DX12", "version": "1.12",
        "source_commit": commit, "source_repository": REPOSITORY,
        "performance_recording_default": False, "automatic_act_reveal": True,
        "native_loot_effects_default": True,
        "native_loot_pickup_default": True,
        "zip_sha256": digest,
        "files": [{"name": name, "bytes": len(data), "sha256": sha256(data)}
                  for name, data in sorted(files.items())],
    }
    # Developer verification output stays beside the ZIP, never inside it.
    (args.output_dir / (NAME + ".manifest.json")).write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    (args.output_dir / "SHA256SUMS.txt").write_text(f"{digest}  {archive.name}\n", encoding="utf-8")
    print(json.dumps({"zip": str(archive), "bytes": archive.stat().st_size,
                      "sha256": digest, "files": sorted(files)}, indent=2))


if __name__ == "__main__":
    main()
