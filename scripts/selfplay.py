#!/usr/bin/env python3
"""Quick self-play smoke test through the UCI interface."""
import subprocess
import sys

p = subprocess.Popen(["./chessengine"], stdin=subprocess.PIPE,
                     stdout=subprocess.PIPE, text=True, bufsize=1)

def send(cmd):
    p.stdin.write(cmd + "\n")
    p.stdin.flush()

def read_until(suffix):
    while True:
        line = p.stdout.readline()
        if not line:
            raise RuntimeError("engine died")
        if suffix in line:
            return line.strip()

send("uci");    read_until("uciok")
send("isready"); read_until("readyok")

moves = []
for ply in range(60):
    send("position startpos moves " + " ".join(moves))
    send("go movetime 150")
    best = read_until("bestmove").split()[1]
    if best == "(none)":
        print("game over at move", len(moves)); break
    moves.append(best)
    print(f"ply {ply+1:2d}: {best}")

print("\nFinal position:", len(moves), "moves played")
send("position startpos moves " + " ".join(moves))
send("d"); print(read_until(" "))
send("quit")
p.wait(timeout=5)
print("self-play OK")
