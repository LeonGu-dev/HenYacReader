"""Repair CMake's misdecoded Chinese MSVC /showIncludes prefix on this toolchain."""

from pathlib import Path
import sys

build = Path(sys.argv[1])
prefix = "注意: 包含文件:  "
misdecoded = prefix.encode("utf-8").decode("cp936")
changed = False
for path in [*build.glob("CMakeFiles/*/CMakeCCompiler.cmake"),
             *build.glob("CMakeFiles/*/CMakeCXXCompiler.cmake"),
             build / "CMakeFiles/rules.ninja"]:
    if not path.exists():
        continue
    source = path.read_text(encoding="utf-8")
    if misdecoded in source:
        path.write_text(source.replace(misdecoded, prefix), encoding="utf-8", newline="\n")
        changed = True

if changed:
    # Zero-dependency entries from earlier builds cannot be trusted.
    (build / ".ninja_deps").unlink(missing_ok=True)
    print("Repaired MSVC include prefix; invalidated old dependency records.")
