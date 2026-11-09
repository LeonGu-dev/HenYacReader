"""Verify reference snapshots, build inputs and an optional clean release payload."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def verify(source: Path, build: Path | None = None, payload: Path | None = None):
    entries = json.loads((source / "legacy/reference-manifest.json").read_text("utf-8"))
    for entry in entries:
        path = source / entry["archive"]
        if hashlib.sha256(path.read_bytes()).hexdigest() != entry["sha256"]:
            raise ValueError(f"Reference snapshot changed: {path}")
    if build:
        ninja = (build / "build.ninja").read_text("utf-8").replace("\\", "/")
        # Ninja inputs, not arbitrary strings in documentation or compiler messages.
        for line in ninja.splitlines():
            if line.startswith("build ") and re.search(r"(?:^|[/ :])legacy/", line, re.I):
                raise ValueError(f"Reference entered build graph: {line}")
        for project in build.rglob("*_lupdate_project.json"):
            if "legacy/reference/" in project.read_text("utf-8").replace("\\", "/"):
                raise ValueError(f"Reference entered translation extraction: {project}")
    for qrc in source.rglob("*.qrc"):
        if any(part in {"legacy", "build-baseline", ".git"} for part in qrc.parts):
            continue
        if re.search(r"(?:^|[/\\>])legacy[/\\]", qrc.read_text("utf-8"), re.I):
            raise ValueError(f"Reference entered Qt resources: {qrc}")
    manifest = []
    if payload:
        apps = {"YACReader.exe", "YACReaderLibrary.exe", "YACReaderLibraryServer.exe"}
        executables = apps | {"vc_redist.x64.exe"}
        roots = {"languages", "translations", "utils", "openssl", "server", "qml",
                 "generic", "iconengines", "imageformats", "networkinformation",
                 "platforms", "platforminputcontexts", "sqldrivers", "styles",
                 "tls", "multimedia", "position", "sensors", "sceneparsers",
                 "geometryloaders", "renderers", "renderplugins", "printsupport", "texttospeech"}
        licenses = {"COPYING.txt", "README.md"}
        forbidden = {".pdb", ".obj", ".lib", ".cpp", ".h", ".ts", ".cbz",
                     ".cbr", ".zip", ".ydb", ".ini", ".log", ".cmd", ".ps1", ".py"}
        files = sorted(path for path in payload.rglob("*") if path.is_file())
        if not apps.issubset({path.name for path in payload.iterdir() if path.is_file()}):
            raise ValueError("Release payload is missing an application")
        for path in files:
            relative = path.relative_to(payload)
            root_file = len(relative.parts) == 1
            allowed = (root_file and (path.name in executables | licenses or path.suffix.lower() == ".dll")) or (
                not root_file and relative.parts[0] in roots)
            if not allowed or path.suffix.lower() in forbidden or any(
                part.lower() in {"legacy", ".git", "tests", ".yacreaderlibrary"} for part in relative.parts
            ) or (path.suffix.lower() == ".exe" and not (root_file and path.name in executables)):
                raise ValueError(f"Unexpected release file: {relative}")
            if path.stem.endswith("_source") and path.suffix == ".qm":
                raise ValueError(f"Translation template entered release: {relative}")
            manifest.append({"path": relative.as_posix(), "bytes": path.stat().st_size,
                             "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
    return {"reference_snapshots": len(entries), "files": manifest,
            "payload_bytes": sum(item["bytes"] for item in manifest)}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--build", type=Path)
    parser.add_argument("--payload", type=Path)
    parser.add_argument("--manifest", type=Path)
    args = parser.parse_args()
    result = verify(args.source, args.build, args.payload)
    if args.manifest:
        args.manifest.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", "utf-8")
    print(f"Isolation verified: {result['reference_snapshots']} snapshots; "
          f"{len(result['files'])} release files; {result['payload_bytes']} bytes")
