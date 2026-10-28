#!/usr/bin/env python3
"""Writes dist/SpiderMan-Source.zip: the project source, no ROMs, builds or private artifacts."""
import fnmatch
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "dist" / "SpiderMan-Source.zip"

INCLUDE = ["CMakeLists.txt", "README.md", "LICENSE", "BUILD_STATUS.md", ".gitignore", ".gitmodules",
           "build_windows.ps1", "build_appimage.sh", "symbols.txt", "sound_symbols.txt",
           "src", "tools", "runtime", "ports", "tests", "packaging", "docs"]

EXCLUDE_DIRS = {".git", "__pycache__", "build", "dist", "rom", "assets", "AppDir"}
EXCLUDE_FILES = ["*.zip", "*.eeprom", "*.ini", "*.pyc", "*.o", "*.obj", "*.a", "*.exe", "*.dll",
                 "*.AppImage", "*.appimage"]


def wanted(path: Path) -> bool:
    if any(part in EXCLUDE_DIRS for part in path.parts):
        return False
    return not any(fnmatch.fnmatch(path.name, pattern) for pattern in EXCLUDE_FILES)


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    count = 0
    with zipfile.ZipFile(OUT, "w", zipfile.ZIP_DEFLATED) as archive:
        for entry in INCLUDE:
            path = ROOT / entry
            if not path.exists():
                continue
            files = [path] if path.is_file() else sorted(path.rglob("*"))
            for file in files:
                if not file.is_file() or not wanted(file.relative_to(ROOT)):
                    continue
                archive.write(file, file.relative_to(ROOT).as_posix())
                count += 1
    print(f"{OUT}  ({count} files, {OUT.stat().st_size:,} bytes)")


if __name__ == "__main__":
    main()
