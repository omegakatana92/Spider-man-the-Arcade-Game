#!/usr/bin/env python3
"""
Name:        run_tests.py
Description: automated tests for the Spider-Man native recompilation: ROM verification, native
             startup, frame-by-frame state and picture comparison against MAME's V60 reference,
             control scripts, and clean shutdown.
Version:     0.1.0
Created:     28-10-2026
Author:      Spider-Man recompilation project

Usage:
  python tests/run_tests.py [--bin DIR] [--rom PATH] [--frames N] [--quick]

Exit code 0 only when every test passes.
"""
import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "build" / "tests"


def run(command, timeout=900, env=None):
    process = subprocess.run(command, cwd=ROOT, text=True, capture_output=True, timeout=timeout, env=env)
    return process.returncode, process.stdout + process.stderr


def child_environment(binaries):
    """PATH for the child processes: the MinGW runtime first on Windows (s32ref is a C++ program and
    needs libstdc++/libgcc from the same toolchain), then what we inherited."""
    env = dict(os.environ)
    if os.name == "nt":
        for candidate in (r"C:\msys64\mingw64\bin", str(binaries)):
            if os.path.isdir(candidate):
                env["PATH"] = candidate + os.pathsep + env.get("PATH", "")
    return env


def first_difference(native, reference):
    a = Path(native).read_text().splitlines()
    b = Path(reference).read_text().splitlines()
    if len(a) != len(b):
        return f"frame count {len(a)} != {len(b)}"
    for index, (left, right) in enumerate(zip(a, b)):
        if left.split()[1:] != right.split()[1:]:
            return f"frame {index}: {left} != {right}"
    return None


def compare(name, native, reference):
    if not Path(native).exists() or not Path(reference).exists():
        return False, f"{name}: missing hash file"
    difference = first_difference(native, reference)
    if difference:
        return False, f"{name}: {difference}"
    frames = len(Path(native).read_text().splitlines())
    return True, f"{name}: {frames}/{frames} identical"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--bin", default=str(ROOT / "build" / "bin"))
    parser.add_argument("--rom", default="")
    parser.add_argument("--frames", type=int, default=0, help="override the script's length")
    parser.add_argument("--quick", action="store_true", help="shorter runs (300 frames)")
    arguments = parser.parse_args()

    binaries = Path(arguments.bin)
    rom = arguments.rom or os.environ.get("SPIDERMAN_ROM", "")
    if not rom:
        if (ROOT / "rom" / "spidman.zip").exists():
            rom = str(ROOT / "rom" / "spidman.zip")
        elif (ROOT / "rom").is_dir():
            rom = str(ROOT / "rom")
        else:
            print("no ROM set: put spidman.zip in rom/ or pass --rom", file=sys.stderr)
            return 2

    headless = binaries / "spidman_headless.exe"
    reference = binaries / "s32ref.exe"
    if not headless.exists():
        headless = binaries / "spidman_headless"
    if not reference.exists():
        reference = binaries / "s32ref"
    if not headless.exists() or not reference.exists():
        print(f"missing {headless} or {reference}; build first", file=sys.stderr)
        return 2

    RESULTS.mkdir(parents=True, exist_ok=True)
    failures = 0

    def report(ok, message):
        nonlocal failures
        print(("PASS  " if ok else "FAIL  ") + message)
        if not ok:
            failures += 1

    # 1. ROM verification
    print("== ROM ==")
    env = child_environment(binaries)
    env["SPIDERMAN_ROM"] = rom
    code, output = run([sys.executable, str(ROOT / "tools" / "verify_rom.py"), rom], env=env)
    report(code == 0 and "20/20" in output, "ROM set 20/20" if code == 0 else output.strip()[-300:])

    # 2. native startup and clean shutdown, no script (attract)
    print("== startup ==")
    attract_frames = 300 if arguments.quick else 1500
    coin_frames = 300 if arguments.quick else 900
    code, output = run([str(headless), "--frames", str(attract_frames),
                        "--hashes", str(RESULTS / "native_attract.txt")], env=env)
    report(code == 0, "native attract run exits cleanly" if code == 0 else f"exit {code}: {output.strip()[-500:]}")
    report("eeprom written:    yes" in output, "EEPROM written by the board")

    # 3. reference and native attract, frame by frame
    print("== attract: state and picture ==")
    code, output = run([str(reference), "--timing", "frames", "--frames", str(attract_frames),
                        "--hashes", str(RESULTS / "ref_attract.txt"),
                        "--video-hashes", str(RESULTS / "ref_attract_video.txt"),
                        "--rom", rom], env=env)
    report(code == 0, "reference attract run" if code == 0 else f"exit {code}: {output.strip()[-500:]}")
    code, output = run([str(headless), "--frames", str(attract_frames),
                        "--hashes", str(RESULTS / "native_attract.txt"),
                        "--video-hashes", str(RESULTS / "native_attract_video.txt")], env=env)
    report(code == 0, "native attract run with video" if code == 0 else f"exit {code}: {output.strip()[-500:]}")
    ok, message = compare("state", RESULTS / "native_attract.txt", RESULTS / "ref_attract.txt")
    report(ok, message)
    ok, message = compare("video", RESULTS / "native_attract_video.txt", RESULTS / "ref_attract_video.txt")
    report(ok, message)

    # 4. coin/start/gameplay script, frame by frame
    print("== coin/start/gameplay ==")
    script = ROOT / "tests" / "controls" / "coin_start.txt"
    code, output = run([str(reference), "--timing", "frames", "--script", str(script),
                        "--hashes", str(RESULTS / "ref_coin.txt"),
                        "--video-hashes", str(RESULTS / "ref_coin_video.txt"),
                        "--rom", rom], env=env)
    report(code == 0, "reference coin/start run" if code == 0 else f"exit {code}: {output.strip()[-500:]}")
    code, output = run([str(headless), "--script", str(script),
                        "--hashes", str(RESULTS / "native_coin.txt"),
                        "--video-hashes", str(RESULTS / "native_coin_video.txt")], env=env)
    report(code == 0, "native coin/start run" if code == 0 else f"exit {code}: {output.strip()[-500:]}")
    ok, message = compare("state", RESULTS / "native_coin.txt", RESULTS / "ref_coin.txt")
    report(ok, message)
    ok, message = compare("video", RESULTS / "native_coin_video.txt", RESULTS / "ref_coin_video.txt")
    report(ok, message)

    # 5. picture dump: a real frame is produced at the expected size
    print("== picture ==")
    frames = RESULTS / "frames"
    if frames.exists():
        shutil.rmtree(frames)
    frames.mkdir()
    code, output = run([str(headless), "--frames", "200", "--video-dump", "120",
                        "--video-dir", str(frames)], env=env)
    picture = frames / "frame_120.bmp"
    ok = code == 0 and picture.exists() and picture.stat().st_size > 100000
    report(ok, "frame 120 drawn (BMP)" if ok else f"exit {code}: {output.strip()[-300:]}")

    # 6. sound: the windowed build with SDL's dummy drivers and the coin/start script must make
    # non-silent samples once gameplay starts. (The sound CPU is the documented reference fallback.)
    print("== sound ==")
    gui = binaries / "spidman.exe"
    if not gui.exists():
        gui = binaries / "spidman"
    if gui.exists():
        sound_env = child_environment(binaries)
        sound_env.update({"SPIDERMAN_ROM": rom, "SPIDERMAN_SOUND_TRACE": "1",
                          "SDL_VIDEODRIVER": "dummy", "SDL_AUDIODRIVER": "dummy"})
        try:
            process = subprocess.Popen([str(gui), "--script", str(script)], cwd=ROOT, env=sound_env,
                                       text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                _, error = process.communicate(timeout=8)
            except subprocess.TimeoutExpired:
                process.kill()
                _, error = process.communicate()
            peaks = [int(line.split("peak")[-1]) for line in error.splitlines() if "peak" in line]
            if peaks:
                report(max(peaks) > 0, f"sound made samples in gameplay (peak {max(peaks)})")
            else:
                print("SKIP  sound: this build has no reference sound fallback")
        except OSError as problem:
            report(False, f"sound launch failed: {problem}")
    else:
        print("SKIP  sound: no windowed build")

    print()
    if failures:
        print(f"FAIL: {failures} test(s) failed")
        return 1
    print("PASS: all tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())

