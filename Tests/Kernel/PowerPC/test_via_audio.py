#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
from array import array
from contextlib import nullcontext
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import wave

sys.dont_write_bytecode = True
import test_desktop


def prepare_root(source, build, user_loader, work):
    root = test_desktop.create_root(source, build, user_loader, work)
    shutil.copy2(build / "Userland/Applications/SoundPlayer/SoundPlayer", root / "bin/SoundPlayer")
    shutil.copy2(build / "Userland/Services/AudioServer/AudioServer", root / "bin/AudioServer")

    sample_rate = 44100
    with wave.open(str(root / "home/anon/test-tone.wav"), "wb") as tone:
        tone.setnchannels(2)
        tone.setsampwidth(2)
        tone.setframerate(sample_rate)
        samples = bytearray()
        for index in range(sample_rate):
            amplitude = 12000 if (index // 50) % 2 == 0 else -12000
            samples.extend(struct.pack("<hh", amplitude, amplitude))
        tone.writeframes(samples)

    with (root / "etc/SystemServerUser.ini").open("a") as config:
        config.write(
            "\n[AudioServer]\n"
            "Socket=/tmp/session/%sid/portal/audio,/tmp/session/%sid/portal/audiomanager\n"
            "SocketPermissions=600\nKeepAlive=false\n"
            "\n[SoundPlayerSmoke]\n"
            "Executable=/bin/SoundPlayer\n"
            "Arguments=/home/anon/test-tone.wav\n"
            "KeepAlive=false\nEnvironment=HOME=/home/anon\n"
        )
    return root


def capture_audio(args, disk, work):
    serial = work / "serial.log"
    captured_audio = work / "captured-audio.wav"
    command = [
        args.qemu, "-M", "pegasos2,audiodev=audio0", "-cpu", args.cpu, "-m", str(args.memory),
        "-vga", "none", "-device", "virtio-gpu-pci", "-device", "virtio-keyboard-pci",
        "-device", "virtio-mouse-pci", "-kernel", str(args.loader), "-append", "root=lun3:0:0",
        "-drive", f"if=none,format=raw,id=virtio_disk,file={disk}",
        "-device", "virtio-blk-pci,drive=virtio_disk",
        "-object", "rng-random,id=rng0,filename=/dev/urandom", "-device", "virtio-rng-pci,rng=rng0",
        "-audiodev", f"wav,id=audio0,path={captured_audio}",
        "-display", "none", "-serial", f"file:{serial}", "-monitor", "stdio", "-no-reboot",
    ]
    process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    try:
        deadline = time.monotonic() + args.timeout
        while time.monotonic() < deadline and process.poll() is None:
            output = serial.read_bytes() if serial.exists() else b""
            if b"KERNEL PANIC" in output or b"CRASH:" in output or b"Exiting with runtime error" in output:
                break
            if b"SoundPlayer(" in output and b"AudioServer(" in output:
                time.sleep(12)
                break
            time.sleep(0.5)
    finally:
        if process.poll() is None:
            try:
                _, errors = process.communicate(input=b"quit\n", timeout=5)
            except subprocess.TimeoutExpired:
                process.terminate()
                _, errors = process.communicate(timeout=5)
        else:
            _, errors = process.communicate(timeout=5)

    output = serial.read_bytes() if serial.exists() else b""
    peak = 0
    frames = 0
    nonzero_samples = 0
    if captured_audio.exists():
        try:
            with captured_audio.open("rb") as audio:
                header = audio.read(44)
                # QEMU's WAV backend may leave both size fields at zero. Use the file length.
                if (len(header) == 44 and header[:4] == b"RIFF" and header[8:16] == b"WAVEfmt "
                        and header[36:40] == b"data"):
                    format_id, channels, sample_rate, _, _, sample_bits = struct.unpack_from("<HHIIHH", header, 20)
                    if format_id == 1 and channels == 2 and sample_rate == 44100 and sample_bits == 16:
                        raw = audio.read()
                        frames = len(raw) // 4
                        samples = array("h")
                        samples.frombytes(raw[:frames * 4])
                        if sys.byteorder != "little":
                            samples.byteswap()
                        peak = max((abs(sample) for sample in samples), default=0)
                        nonzero_samples = sum(sample != 0 for sample in samples)
        except OSError:
            pass
    if (frames < 1000 or peak < 1000 or nonzero_samples < 10000
            or b"VIA AC'97" not in output or b"Couldn't open first audio channel" in output
            or b"KERNEL PANIC" in output or b"CRASH:" in output
            or b"Exiting with runtime error" in output):
        raise SystemExit(
            f"Pegasos 2 VIA audio output failed: frames={frames}, peak={peak}, nonzero={nonzero_samples}, "
            f"capture={captured_audio}, serial={serial}\n"
            + output.decode(errors="replace")[-4000:] + errors.decode(errors="replace")[-1000:]
        )
    print(f"Pegasos 2 VIA AC'97 played {nonzero_samples} nonzero samples (peak {peak}) across {frames} captured frames")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--loader", type=Path, required=True)
    parser.add_argument("--user-loader", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--cpu", default="750cxe")
    parser.add_argument("--memory", type=int, default=512)
    parser.add_argument("--timeout", type=int, default=90)
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()

    source = Path(__file__).parents[3]
    parent = args.output_dir or Path("/tmp")
    parent.mkdir(parents=True, exist_ok=True)
    directory_context = (nullcontext(tempfile.mkdtemp(prefix="powerpc-via-audio-", dir=parent))
                         if args.output_dir else tempfile.TemporaryDirectory(prefix="powerpc-via-audio-", dir=parent))
    with directory_context as directory:
        work = Path(directory)
        root = work / "root"
        disk = work / "root.ext2"
        try:
            root = prepare_root(source, args.build_dir, args.user_loader, work)
            disk = test_desktop.create_disk(root, work)
            capture_audio(args, disk, work)
        finally:
            if args.output_dir:
                shutil.rmtree(root, ignore_errors=True)
                disk.unlink(missing_ok=True)


if __name__ == "__main__":
    main()
