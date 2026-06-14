#!/usr/bin/env python3
"""qemu-shot.py — capture l'écran d'un QEMU que TU as lancé. C'est tout.

Tu lances QEMU avec un socket QMP (`make run QMP=1`), ce script s'y attache,
capture l'écran dans un PNG, et te le dit. Tu ouvres l'image et tu juges
toi-même — pas de PASS/FAIL, pas d'OCR. Réutilisable à chaque brique.

Le seul morceau « malin » : on n'utilise PAS un `sleep` fixe avant la capture.
Le boot est instantané en temps *guest* mais QMP répond avant que SeaBIOS ait
fini en temps *réel* ; capturer trop tôt donne « display not initialized ». On
attend donc que l'écran se STABILISE. Comme le curseur « _ » du mode texte
clignote pour toujours (deux frames ne sont jamais identiques au bit près), on
compare la TAILLE des captures (clignotement ≈ 0,5 %, toléré), pas les octets.

Usage :
  make run QMP=1                # terminal 1 : QEMU headless + socket QMP
  python3 tools/qemu-shot.py    # terminal 2 : capture build/shot.png
"""
import argparse, json, os, socket, sys, time


def recv_line(sock):
    buf = b""
    while b"\n" not in buf:
        chunk = sock.recv(4096)
        if not chunk:
            raise ConnectionError("socket QMP fermé par QEMU")
        buf += chunk
    return buf


def cmd(sock, obj):
    sock.sendall((json.dumps(obj) + "\n").encode())
    return json.loads(recv_line(sock))


def main():
    p = argparse.ArgumentParser(description="Capture l'écran d'un QEMU lancé avec un socket QMP.")
    p.add_argument("--sock", default="/tmp/naos-qmp.sock", help="socket QMP UNIX")
    p.add_argument("--shot", default="build/shot.png", help="PNG de sortie")
    p.add_argument("--timeout", type=float, default=10.0, help="délai max de stabilisation (s)")
    a = p.parse_args()
    os.makedirs(os.path.dirname(a.shot) or ".", exist_ok=True)

    # S'attacher au QEMU déjà lancé (petit retry si on part une fraction trop tôt).
    start = time.time()
    sock = None
    while time.time() - start < 3.0:
        try:
            sock = socket.socket(socket.AF_UNIX); sock.connect(a.sock)
            break
        except OSError:
            time.sleep(0.05)
    if sock is None:
        sys.exit(f"Aucun QEMU sur {a.sock}. Lance-le d'abord :  make run QMP=1")
    recv_line(sock)                                        # greeting QMP
    cmd(sock, {"execute": "qmp_capabilities"})

    # Capturer en boucle jusqu'à 2 captures consécutives de taille proche (stable).
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

    print(f"capture écrite : {a.shot} — ouvre-la pour vérifier.")


if __name__ == "__main__":
    main()
