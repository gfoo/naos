#!/usr/bin/env python3
"""qemu-shot.py — capture the screen of a QEMU that YOU launched. That's all.

You launch QEMU with a QMP socket (`make run-b3 QMP=1`), this script attaches to it,
captures the screen into a PNG, and tells you. You open the image and judge it
yourself — no PASS/FAIL, no OCR. Reusable for every brick.

The only "clever" part: we do NOT use a fixed `sleep` before the capture.
The boot is instantaneous in *guest* time but QMP answers before SeaBIOS has
finished in *real* time; capturing too early gives "display not initialized". So
we wait for the screen to STABILIZE. Since the text-mode cursor "_" blinks
forever (two frames are never identical down to the bit), we compare the SIZE of
the captures (blink ≈ 0.5%, tolerated), not the bytes.

Usage:
  make run-b3 QMP=1                # terminal 1: headless QEMU + QMP socket
  python3 tools/qemu-shot.py    # terminal 2: capture build/shot.png
"""
import argparse, json, os, socket, sys, time


def recv_line(sock):
    buf = b""
    while b"\n" not in buf:
        chunk = sock.recv(4096)
        if not chunk:
            raise ConnectionError("QMP socket closed by QEMU")
        buf += chunk
    return buf


def cmd(sock, obj):
    sock.sendall((json.dumps(obj) + "\n").encode())
    return json.loads(recv_line(sock))


def main():
    p = argparse.ArgumentParser(description="Capture the screen of a QEMU launched with a QMP socket.")
    p.add_argument("--sock", default="/tmp/naos-qmp.sock", help="QMP UNIX socket")
    p.add_argument("--shot", default="build/shot.png", help="output PNG")
    p.add_argument("--timeout", type=float, default=10.0, help="max stabilization delay (s)")
    a = p.parse_args()
    os.makedirs(os.path.dirname(a.shot) or ".", exist_ok=True)

    # Attach to the already-launched QEMU (small retry in case we start a fraction too early).
    start = time.time()
    sock = None
    while time.time() - start < 3.0:
        try:
            sock = socket.socket(socket.AF_UNIX); sock.connect(a.sock)
            break
        except OSError:
            time.sleep(0.05)
    if sock is None:
        sys.exit(f"No QEMU on {a.sock}. Launch it first:  make run-b3 QMP=1")
    recv_line(sock)                                        # QMP greeting
    cmd(sock, {"execute": "qmp_capabilities"})

    # Capture in a loop until 2 consecutive captures of close size (stable).
    start, prev, matches = time.time(), None, 0
    while time.time() - start < a.timeout:
        cmd(sock, {"execute": "screendump",
                   "arguments": {"filename": a.shot, "format": "png"}})
        n = len(open(a.shot, "rb").read())
        if n >= 1024 and prev is not None and abs(n - prev) <= max(128, n // 20):
            matches += 1
            if matches >= 2:
                break
        else:
            matches = 0
        prev = n
        time.sleep(0.3)

    print(f"capture written: {a.shot} — open it to verify.")


if __name__ == "__main__":
    main()
