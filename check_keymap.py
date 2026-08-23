#!/usr/bin/env python3
"""Sofle Choc Pro has 60 keys. Every layer must bind exactly 60. Run: python3 check_keymap.py"""
import re, sys, pathlib

KEYS = 60
src = pathlib.Path("config/sofle_choc_pro.keymap").read_text()
keymap = src[src.index("keymap {"):]

layers = re.findall(r"(\w+_layer)\s*\{(.*?)bindings\s*=\s*<(.*?)>;(.*?)\n    \};", keymap, re.S)
assert len(layers) == 4, f"found {len(layers)} layers, expected 4"

bad = 0
for name, _, binds, tail in layers:
    n = len(re.findall(r"&\w+", binds))
    print(f"{name:14} {n:3} bindings")
    if n != KEYS:
        print(f"  ^ expected {KEYS}", file=sys.stderr)
        bad += 1
    s = len(re.findall(r"&\w+", tail))
    if s != 2:
        print(f"  ^ {name}: {s} sensor bindings, expected 2", file=sys.stderr)
        bad += 1
sys.exit(bad)

