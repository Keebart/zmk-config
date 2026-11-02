#!/usr/bin/env python3
"""
Render a ZMK keymap as a standalone HTML page, one board per layer, plus the
combos drawn on top of the keys that trigger them.

Geometry comes from the matching config/<name>.json, so what you see is the real
column stagger -- not an idealised grid. The Nth entry of that file's layout
array is key position N, which is also the Nth binding in every layer and what
`key-positions` in a combo refers to.

Usage:
    python3 scripts/draw_keymap.py -k config/corne_v2.keymap \
                                   -j config/corne_v2.json

Requires nothing outside the standard library.
"""

import argparse
import json
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
DEFAULT_KEYMAP = os.path.join(REPO, "config", "corne_v2.keymap")
DEFAULT_LAYOUT = os.path.join(REPO, "config", "corne_v2.json")

U = 62          # px per key unit
GAP = 3         # px shaved off each key so neighbours don't touch


# --------------------------------------------------------------------------
# parsing
# --------------------------------------------------------------------------

def read_layers(path):
    """Return [(layer_name, display_name, [binding, ...]), ...] in file order."""
    src = open(path, encoding="utf-8").read()
    src = src[src.index("keymap {"):]

    layers = []
    # display-name is optional -- the bluetooth layer has none, and a layer that
    # does not announce itself is still a layer.
    for m in re.finditer(r"(\w+)\s*\{\s*(?:display-name = \"([^\"]*)\";\s*)?bindings = <", src):
        name, disp = m.group(1), m.group(2) or m.group(1)
        start = m.end()
        end = src.index(">;", start)
        body = src[start:end]
        # every binding starts with '&'; split on that and re-attach
        parts = [("&" + p).strip() for p in body.split("&") if p.strip()]
        parts = [re.sub(r"\s+", " ", p) for p in parts]
        layers.append((name, disp, parts))
    return layers


def read_layout(path):
    d = json.load(open(path, encoding="utf-8"))
    layouts = d["layouts"]
    name = "LAYOUT" if "LAYOUT" in layouts else sorted(layouts)[0]
    return layouts[name]["layout"], d.get("sensors", [])


def read_behaviors(path):
    """
    -> {name: ("morph", tap_binding, shifted_binding)}
     | {name: ("glyph", character)}

    Both are read out of the keymap rather than hardcoded. Mod-morphs give the
    two glyphs a keycap should show. Unicode macros spell their codepoint in the
    hex digits they type under Option -- &kp N2 &kp N0 &kp A &kp C is U+20AC --
    so the glyph can simply be decoded instead of maintained in a lookup table.
    """
    src = open(path, encoding="utf-8").read()
    out = {}
    for m in re.finditer(r"(\w+):\s*\w+\s*\{([^{}]*?)\}", src, re.S):
        name, body = m.group(1), m.group(2)
        if 'compatible = "zmk,behavior-mod-morph"' in body:
            b = re.search(r"bindings = <([^>]*)>\s*,\s*<([^>]*)>;", body)
            if b:
                out[name] = ("morph", b.group(1).strip(), b.group(2).strip())
        elif 'compatible = "zmk,behavior-macro"' in body:
            t = re.search(r"macro_tap\s+((?:&kp\s+\w+\s*)+)", body)
            if not t:
                continue
            digits = ""
            for tok in re.findall(r"&kp\s+(\w+)", t.group(1)):
                if re.match(r"^N\d$", tok):
                    digits += tok[1]
                elif re.match(r"^[A-F]$", tok):
                    digits += tok
                else:
                    digits = ""
                    break
            if len(digits) == 4:
                out[name] = ("glyph", chr(int(digits, 16)))
    return out


def read_combos(path):
    """-> [(name, [positions], binding, [layers] or None)]
    Properties are read out one at a time rather than matched in a fixed order:
    devicetree does not care about ordering, and insisting on one meant a single
    added property (require-prior-idle-ms) silently dropped every combo. `layers`
    is optional too -- a combo without it is global in ZMK, and comes back here as
    None for the caller to expand once it knows how many layers there are.
    """
    src = open(path, encoding="utf-8").read()
    try:
        block = src[src.index("combos {"):src.index("keymap {")]
    except ValueError:
        return []
    out = []
    for m in re.finditer(r"(\w+)\s*\{(.*?)\};", block, re.S):
        name, body = m.group(1), m.group(2)
        pos = re.search(r"key-positions = <([^>]*)>", body)
        bind = re.search(r"bindings = <([^>]*)>", body)
        if not (pos and bind):
            continue
        lyr = re.search(r"layers = <([^>]*)>", body)
        out.append((name,
                    [int(x) for x in pos.group(1).split()],
                    bind.group(1).strip(),
                    [int(x) for x in lyr.group(1).split()] if lyr else None))
    return out


# --------------------------------------------------------------------------
# turning a ZMK binding into something readable on a keycap
# --------------------------------------------------------------------------

KEY_LABEL = {
    "N0": "0", "N1": "1", "N2": "2", "N3": "3", "N4": "4",
    "N5": "5", "N6": "6", "N7": "7", "N8": "8", "N9": "9",
    "SEMI": ";", "COMMA": ",", "DOT": ".", "FSLH": "/", "BSLH": "\\",
    "MINUS": "-", "PLUS": "+", "EQUAL": "=", "UNDER": "_", "STAR": "*",
    "AMPS": "&", "PIPE": "|", "TILDE": "~", "GRAVE": "`", "CARET": "^",
    "DLLR": "$", "PRCNT": "%", "HASH": "#", "AT": "@", "EXCL": "!",
    "QMARK": "?", "COLON": ":", "LT": "<", "GT": ">",
    "LPAR": "(", "RPAR": ")", "LBRC": "{", "RBRC": "}",
    "LBKT": "[", "RBKT": "]",
    "SPACE": "Space", "ENTER": "Enter", "TAB": "Tab", "BSPC": "Bspc",
    "DEL": "Del", "ESC": "Esc", "CAPS": "Caps",
    "UP": "↑", "DOWN": "↓", "LEFT": "←", "RIGHT": "→",
    "HOME": "Home", "END": "End", "PG_UP": "PgUp", "PG_DN": "PgDn",
    "LSHFT": "Shift", "RSHFT": "Shift", "LCTRL": "Ctrl", "RCTRL": "Ctrl",
    "LALT": "Alt", "RALT": "Alt", "LMETA": "Cmd", "RMETA": "Cmd",
    "C_MUTE": "Mute", "C_VOL_UP": "Vol+", "C_VOL_DN": "Vol-",
    "C_NEXT": "Next", "C_PREV": "Prev",
    "C_BRI_UP": "Bri+", "C_BRI_DN": "Bri-",
    "C_PP": "Play", "C_PLAY_PAUSE": "Play", "C_SLEEP": "Sleep",
    "RET": "Enter", "LGUI": "Cmd", "RGUI": "Cmd", "GLOBE": "Globe",
    # keycodes whose names are longer than the glyph they produce
    "ASTRK": "*", "DQT": '"', "SQT": "'", "EXCL": "!", "AMPS": "&",
    "PRCNT": "%", "CARET": "^", "DLLR": "$", "HASH": "#", "AT": "@",
    "QMARK": "?", "UNDER": "_", "TILDE": "~", "GRAVE": "`", "PIPE": "|",
    "LBRC": "{", "RBRC": "}", "LPAR": "(", "RPAR": ")",
    "LBKT": "[", "RBKT": "]", "BSLH": "\\", "FSLH": "/",
    # the keymap-editor bot writes the long alias for these, so both spellings
    # have to resolve or the cap reads "Slash" / "Question" / "Period".
    "SLASH": "/", "QUESTION": "?", "PERIOD": ".", "ASTERISK": "*",
    "DOLLAR": "$", "SEMICOLON": ";", "APOSTROPHE": "'", "EXCLAMATION": "!",
    # mac Option chords -- these produce a glyph on the standard U.S. layout, so
    # the cap should show the glyph rather than the keycode that generates it.
    # (This is how SYMBOL types € ° ± ß § ∆ without Unicode Hex Input, which
    # would otherwise swallow Option and break opt+arrow word navigation.)
    "LS(LA(N2))": "€", "LS(LA(N8))": "°", "LS(LA(EQUAL))": "±",
    "LA(S)": "ß", "LA(N6)": "§", "LA(J)": "∆",
    "LC(LG(Q))": "Lock",
}

LAYER_NAMES = {0: "base", 1: "symbol", 2: "numnav"}

# What a plain US key gives you with Shift held. Drawn small above the tap glyph
# so a key reads like the cap it is: "<" over ",", "?" over "/".
#
# Digits are deliberately absent. They shift to !@#$%^&*() on a US board, but the
# only digits here are the NUMNAV numpad, where nobody shifts and the extra row
# of glyphs would just be noise. Add them here if you disagree.
SHIFT_PAIR = {
    "COMMA": "<", "DOT": ">", "FSLH": "?", "SEMI": ":", "SQT": '"',
    "GRAVE": "~", "MINUS": "_", "EQUAL": "+", "BSLH": "|",
    "LBKT": "{", "RBKT": "}",
    "SLASH": "?", "PERIOD": ">", "SEMICOLON": ":", "APOSTROPHE": '"',
}



# behaviours whose name is the whole story
NAMED = {
    "comma_morph": (",", "; <"),
    "dot_morph": (".", ": >"),
    "qexcl": ("?", "!"),
    "bksp_esc_morph": ("Bspc", "Esc"),
    "bt_clr": ("BT clr", "all"),
    "td_to_game": ("⇥⇥", "game"),
    "magic_shift_tap": ("Shift", "caps"),
}


def label(key):
    return KEY_LABEL.get(key, key.replace("_", " ").title() if len(key) > 3 else key)


def pretty(binding, beh_map=None):
    """
    -> (main, sub, kind, sub_above)

    `sub_above` is what makes a key read like a real keycap: for a mod-morph the
    Shift glyph belongs ABOVE the tap glyph, the way it is printed on the cap.
    """
    beh_map = beh_map or {}
    toks = binding.split()
    beh, args = toks[0], toks[1:]

    bare = beh.lstrip("&")
    if bare in beh_map:
        entry = beh_map[bare]
        if entry[0] == "glyph":
            return (entry[1], "", "uni", False)
        # mod-morph: entry = ("morph", tap, shifted) -> the Shift glyph sits above
        tap = pretty(entry[1], beh_map)[0]
        shifted = pretty(entry[2], beh_map)[0]
        return (tap, shifted, "morph", True)

    if beh == "&trans":
        return ("▽", "", "trans", False)
    if beh == "&none":
        return ("✕", "", "none", False)
    if beh == "&kp":
        shifted = SHIFT_PAIR.get(args[0])
        if shifted:
            return (label(args[0]), shifted, "kp", True)
        return (label(args[0]), "", "kp", False)
    if beh in ("&hrm_l", "&hrm_r"):
        return (label(args[1]), label(args[0]), "hold", False)
    if beh == "&lt":
        lyr = LAYER_NAMES.get(int(args[0]), args[0]) if args[0].isdigit() else args[0].lower()
        return (label(args[1]), lyr, "hold", False)
    if beh == "&magic_shift":
        return (label(args[0]), "sticky", "hold", False)
    if beh == "&mo":
        lyr = LAYER_NAMES.get(int(args[0]), args[0]) if args[0].isdigit() else args[0].lower()
        return (lyr, "hold", "layer", False)
    if beh == "&to":
        lyr = LAYER_NAMES.get(int(args[0]), args[0]) if args[0].isdigit() else args[0].lower()
        return (lyr, "to", "layer", False)
    if beh == "&sl":
        return (args[0], "sticky", "layer", False)
    if beh == "&bt":
        return (" ".join(args).replace("BT_SEL", "BT").replace("BT_CLR_ALL", "BT clr all")
                .replace("BT_CLR", "BT clr"), "", "sys", False)
    if beh == "&caps_word":
        return ("Caps", "word", "sys", False)
    name = beh.lstrip("&")
    if name in NAMED:
        m, s = NAMED[name]
        return (m, s, "morph", True)
    return (name.replace("_", " "), "", "custom", False)


# --------------------------------------------------------------------------
# geometry
# --------------------------------------------------------------------------

def corners(k):
    """Four corners of a key in px, after KLE-style rotation."""
    x, y = k["x"] * U, k["y"] * U
    w, h = k.get("w", 1) * U, k.get("h", 1) * U
    pts = [(x, y), (x + w, y), (x + w, y + h), (x, y + h)]
    r = k.get("r", 0)
    if r:
        cx, cy = k["rx"] * U, k["ry"] * U
        a = math.radians(r)
        ca, sa = math.cos(a), math.sin(a)
        pts = [((px - cx) * ca - (py - cy) * sa + cx,
                (px - cx) * sa + (py - cy) * ca + cy) for px, py in pts]
    return pts


def bbox(layout):
    xs, ys = [], []
    for k in layout:
        for px, py in corners(k):
            xs.append(px)
            ys.append(py)
    return min(xs), min(ys), max(xs), max(ys)


def svg_for(layout, bindings, sensor_pos, unbound, combos=(), layer_idx=0,
            beh_map=None, stack=None):
    x0, y0, x1, y1 = bbox(layout)
    pad = 14
    w, h = (x1 - x0) + 2 * pad, (y1 - y0) + 2 * pad
    out = ['<svg class="board" viewBox="%.1f %.1f %.1f %.1f" role="img">'
           % (x0 - pad, y0 - pad, w, h)]

    for i, k in enumerate(layout):
        b = bindings[i] if i < len(bindings) else "&none"
        inherited = False
        # A transparent key is not blank -- it is whatever the layer below puts
        # there. Show that, dimmed, so the picture matches the layout document.
        #
        # Resolve against the BASE layer, not the next layer down. &trans falls
        # through to the highest layer that is currently ACTIVE, and these layers
        # are independent momentary holds -- you are on exactly one at a time, so
        # the thing underneath is always base. Walking down one layer at a time
        # would report SYMBOL's &none for a key that in practice gives you base.
        if b == "&trans" and stack and layer_idx > 0:
            if stack[0][i] != "&trans":
                b = stack[0][i]
                inherited = True
        main, sub, kind, sub_above = pretty(b, beh_map)
        cls = ["key", "k-" + kind]
        if inherited:
            cls.append("k-inherit")
        if i in unbound:
            cls.append("k-open")
        if any(i in c[1] for c in combos if layer_idx in c[3]):
            cls.append("k-combo")
        if (k["row"], k["col"]) in sensor_pos:
            cls.append("k-enc")

        x, y = k["x"] * U, k["y"] * U
        tr = ""
        if k.get("r"):
            tr = ' transform="rotate(%.3f %.2f %.2f)"' % (k["r"], k["rx"] * U, k["ry"] * U)

        out.append('<g class="%s"%s>' % (" ".join(cls), tr))
        kw, kh = k.get("w", 1) * U, k.get("h", 1) * U
        out.append('<rect x="%.2f" y="%.2f" width="%.2f" height="%.2f" rx="7"/>'
                   % (x + GAP, y + GAP, kw - 2 * GAP, kh - 2 * GAP))
        cx = x + kw / 2
        if sub and sub_above:
            # keycap order: Shift glyph on top, tap glyph below
            out.append('<text class="sub" x="%.2f" y="%.2f">%s</text>' % (cx, y + kh * 0.36, esc(sub)))
            out.append('<text class="main" x="%.2f" y="%.2f">%s</text>' % (cx, y + kh * 0.74, esc(main)))
        elif sub:
            out.append('<text class="main" x="%.2f" y="%.2f">%s</text>' % (cx, y + kh * 0.46, esc(main)))
            out.append('<text class="sub" x="%.2f" y="%.2f">%s</text>' % (cx, y + kh * 0.73, esc(sub)))
        else:
            out.append('<text class="main solo" x="%.2f" y="%.2f">%s</text>' % (cx, y + kh * 0.58, esc(main)))
        out.append('<text class="pos" x="%.2f" y="%.2f">%d</text>' % (x + kw - 8, y + 15, i))
        out.append("</g>")

    # combo links: a tie-line between the two keys plus the glyph it produces
    for name, pos, bind, lyrs in combos:
        if layer_idx not in lyrs or len(pos) != 2 or max(pos) >= len(layout):
            continue
        cs = []
        for p in pos:
            k = layout[p]
            kx, ky = k["x"] * U, k["y"] * U
            cx, cy = kx + k.get("w", 1) * U / 2, ky + k.get("h", 1) * U / 2
            if k.get("r"):
                a = math.radians(k["r"])
                ox, oy = k["rx"] * U, k["ry"] * U
                cx, cy = ((cx - ox) * math.cos(a) - (cy - oy) * math.sin(a) + ox,
                          (cx - ox) * math.sin(a) + (cy - oy) * math.cos(a) + oy)
            cs.append((cx, cy))
        (x1, y1), (x2, y2) = cs
        mx, my = (x1 + x2) / 2, (y1 + y2) / 2
        glyph = bind.replace("&kp ", "").strip()
        glyph = label(glyph)   # KEY_LABEL already knows every glyph a combo emits
        out.append('<g class="combo">')
        out.append('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f"/>' % (x1, y1, x2, y2))
        if len(glyph) > 1:
            w = 9 * len(glyph) + 10
            out.append('<rect x="%.1f" y="%.1f" width="%.1f" height="24" rx="12"/>'
                       % (mx - w / 2, my - 12, w))
        else:
            out.append('<circle cx="%.1f" cy="%.1f" r="12"/>' % (mx, my))
        out.append('<text x="%.1f" y="%.1f">%s</text>' % (mx, my + 5, esc(glyph)))
        out.append("</g>")

    out.append("</svg>")
    return "".join(out)


def esc(s):
    return (s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))


# --------------------------------------------------------------------------

CSS = """
:root{
  --bg:#f4f2ee; --panel:#fffefb; --ink:#1b1d21; --ink-2:#5d6470; --ink-3:#8b93a1;
  --rule:#ddd8d0; --cap:#fffefb; --cap-edge:#cfc9c0; --cap-ink:#1b1d21;
  --trans-cap:#eae6e0; --trans-ink:#a9a396;
  --accent:#c2410c; --accent-soft:#fde5d3; --enc:#0e7490; --enc-soft:#cff2f7;
  --shadow:0 1px 0 rgba(0,0,0,.10), 0 2px 5px rgba(0,0,0,.05);
}
@media (prefers-color-scheme:dark){
  :root:not([data-theme="light"]){
    --bg:#101318; --panel:#171b22; --ink:#e8e6e1; --ink-2:#a2aab8; --ink-3:#6b7280;
    --rule:#262c36; --cap:#232932; --cap-edge:#333b47; --cap-ink:#eceae5;
    --trans-cap:#1a1f26; --trans-ink:#5a626e;
    --accent:#fb923c; --accent-soft:#3a2413; --enc:#22d3ee; --enc-soft:#0c2e36;
    --shadow:0 1px 0 rgba(0,0,0,.5), 0 2px 6px rgba(0,0,0,.35);
  }
}
:root[data-theme="dark"]{
  --bg:#101318; --panel:#171b22; --ink:#e8e6e1; --ink-2:#a2aab8; --ink-3:#6b7280;
  --rule:#262c36; --cap:#232932; --cap-edge:#333b47; --cap-ink:#eceae5;
  --trans-cap:#1a1f26; --trans-ink:#5a626e;
  --accent:#fb923c; --accent-soft:#3a2413; --enc:#22d3ee; --enc-soft:#0c2e36;
  --shadow:0 1px 0 rgba(0,0,0,.5), 0 2px 6px rgba(0,0,0,.35);
}

*{box-sizing:border-box}
body{
  margin:0; background:var(--bg); color:var(--ink);
  font-family:"IBM Plex Sans Condensed","Avenir Next Condensed",system-ui,sans-serif;
  font-size:16px; line-height:1.5;
}
.wrap{max-width:1180px; margin:0 auto; padding:32px 22px 80px; display:flex; flex-direction:column; gap:30px}

header{display:flex; flex-direction:column; gap:8px; border-bottom:2px solid var(--ink); padding-bottom:16px}
h1{margin:0; font-size:1.7rem; font-weight:650; letter-spacing:-.015em}
.meta{color:var(--ink-2); font-size:.9rem; display:flex; gap:16px; flex-wrap:wrap;
      font-variant-numeric:tabular-nums}
.meta b{color:var(--ink); font-weight:600}

.legend{display:flex; gap:18px; flex-wrap:wrap; align-items:center; font-size:.82rem; color:var(--ink-2)}
.swatch{display:inline-flex; align-items:center; gap:7px}
.chip{width:15px; height:15px; border-radius:4px; border:1px solid var(--cap-edge); background:var(--cap)}
.chip.n{background:var(--accent-soft); border-color:var(--accent)}
.chip.e{background:var(--enc-soft); border-color:var(--enc)}
.chip.t{background:var(--trans-cap)}
.chip.o{background:var(--trans-cap); border-color:var(--accent); border-style:dashed}
.chip.c{background:var(--enc-soft); border-color:var(--enc)}

section{background:var(--panel); border:1px solid var(--rule); border-radius:12px;
        padding:18px 18px 6px; box-shadow:var(--shadow)}
.lname{display:flex; align-items:baseline; gap:10px; margin-bottom:6px}
.lname h2{margin:0; font-size:1.05rem; font-weight:650; letter-spacing:.02em; text-transform:uppercase}
.lname .idx{font:600 .78rem/1 "IBM Plex Mono",ui-monospace,monospace; color:var(--ink-3);
            border:1px solid var(--rule); border-radius:20px; padding:4px 9px}
.scroll{overflow-x:auto; padding-bottom:12px}
svg.board{display:block; width:100%; min-width:760px; height:auto}

.key rect{fill:var(--cap); stroke:var(--cap-edge); stroke-width:1}
.key text{text-anchor:middle; fill:var(--cap-ink);
          font-family:"IBM Plex Mono",ui-monospace,SFMono-Regular,monospace}
.main{font-size:15px; font-weight:500}
.main.solo{font-size:16px}
.sub{font-size:10.5px; fill:var(--ink-2); font-weight:400; letter-spacing:.02em}
.pos{font-size:8.5px; fill:var(--ink-3); text-anchor:end; font-weight:400}

.k-trans rect{fill:var(--trans-cap); stroke:var(--rule)}
.k-trans .main{fill:var(--trans-ink); font-size:13px}
.k-none rect{fill:var(--trans-cap); stroke:var(--rule); stroke-dasharray:3 2}
.k-none .main{fill:var(--trans-ink)}
.k-layer rect,.k-sys rect{fill:var(--enc-soft); stroke:var(--enc)}
.k-morph .main{font-size:12.5px}

.k-new rect{fill:var(--accent-soft); stroke:var(--accent); stroke-width:1.8}
.k-new .main{fill:var(--accent); font-weight:600}
.k-open rect{fill:var(--trans-cap); stroke:var(--accent); stroke-width:1; stroke-dasharray:4 3}
.k-open .main{fill:var(--accent); opacity:.75}
.k-combo rect{stroke:var(--enc); stroke-width:1.6}
.k-inherit rect{fill:var(--trans-cap)}
.k-inherit .main{fill:var(--ink-3)}
.k-inherit .sub{fill:var(--ink-3); opacity:.7}
.k-uni .main{font-size:19px}
.combo line{stroke:var(--enc); stroke-width:2.4; stroke-linecap:round; opacity:.5}
.combo circle,.combo rect{fill:var(--enc); stroke:var(--panel); stroke-width:2}
.combo text{text-anchor:middle; fill:var(--panel); font:600 14px "IBM Plex Mono",ui-monospace,monospace}
.k-enc rect{stroke:var(--enc); stroke-width:1.8; stroke-dasharray:5 3}

footer{color:var(--ink-2); font-size:.85rem; border-top:1px solid var(--rule); padding-top:16px}
code{font-family:"IBM Plex Mono",ui-monospace,monospace; font-size:.86em;
     background:var(--trans-cap); padding:1px 5px; border-radius:4px}
"""


def build_html(layers, layout, sensors, unbound, title, combos=(), beh_map=None):
    sensor_pos = set((s["row"], s["col"]) for s in sensors)
    parts = []
    stack = [b for _n, _d, b in layers]
    for name, disp, bindings in layers:
        idx = len(parts)
        parts.append(
            '<section><div class="lname"><h2>%s</h2><span class="idx">layer %d &middot; %s</span></div>'
            '<div class="scroll">%s</div></section>'
            % (esc(disp), idx, esc(name),
               svg_for(layout, bindings, sensor_pos, unbound, combos, idx,
                       beh_map, stack))
        )

    combo_txt = " &middot; ".join(
        "%s = %s" % ("+".join(str(p) for p in c[1]),
                     esc(c[2].replace("&kp ", "")))
        for c in combos)
    return """<title>%s</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500;600&family=IBM+Plex+Sans+Condensed:wght@400;500;600;700&display=swap" rel="stylesheet">
<style>%s</style>
<div class="wrap">
<header>
  <h1>%s</h1>
  <div class="meta">
    <span><b>%d</b> keys</span><span><b>%d</b> per half</span>
    <span><b>%d</b> layers</span><span><b>%d</b> combos</span>
  </div>
  <div class="legend">
    <span class="swatch"><i class="chip"></i>bound</span>
    <span class="swatch"><i class="chip t"></i>&#9661; transparent</span>
    <span class="swatch"><i class="chip o"></i>unbound on every layer</span>
    <span class="swatch"><i class="chip c"></i>part of a combo</span>
    <span>keycap order: Shift glyph above, tap glyph below</span>
    <span>small number = key position</span>
  </div>
</header>
%s
<footer>
  Geometry from the matching <code>config/*.json</code>; bindings and combos
  parsed from the <code>.keymap</code>. Key position <i>N</i> is the <i>N</i>th
  binding in every layer and what a combo's <code>key-positions</code> names.
  Combos: %s. Regenerate with <code>python3 scripts/draw_keymap.py</code>.
</footer>
</div>""" % (esc(title), CSS, esc(title), len(layout), len(layout) // 2,
             len(layers), len(combos), "".join(parts), combo_txt)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-k", "--keymap", default=DEFAULT_KEYMAP)
    ap.add_argument("-j", "--layout", default=DEFAULT_LAYOUT)
    # Default the output name from the INPUT name, so rendering two keymaps in
    # the same repo does not have the second silently overwrite the first.
    ap.add_argument("-o", "--out", default=None,
                    help="output .html (default: <repo>/<keymap name>.html)")
    ap.add_argument("-t", "--title", default=None,
                    help="page title (default: derived from the keymap name)")
    a = ap.parse_args()

    stem = os.path.splitext(os.path.basename(a.keymap))[0]
    if a.out is None:
        a.out = os.path.join(REPO, stem + ".html")
    if a.title is None:
        a.title = stem.replace("_", " ").title() + " Keymap"

    layers = read_layers(a.keymap)
    layout, sensors = read_layout(a.layout)
    combos = read_combos(a.keymap)
    # a combo with no `layers` runs on every layer, so draw it on every layer
    combos = [(n, pos, b, list(range(len(layers))) if lyrs is None else lyrs)
              for n, pos, b, lyrs in combos]
    beh_map = read_behaviors(a.keymap)

    if not layers:
        sys.exit("no layers parsed from %s" % a.keymap)
    for name, _disp, b in layers:
        if len(b) != len(layout):
            sys.exit("layer %s has %d bindings but the layout has %d keys"
                     % (name, len(b), len(layout)))

    # positions that are &trans on every single layer -- i.e. genuinely unbound
    unbound = set(range(len(layout)))
    for _n, _d, b in layers:
        unbound &= set(i for i, x in enumerate(b) if x == "&trans")

    open(a.out, "w", encoding="utf-8").write(
        build_html(layers, layout, sensors, unbound, a.title, combos, beh_map))
    print("wrote %s  (%d layers, %d keys, %d combos, %d behaviours decoded)"
          % (a.out, len(layers), len(layout), len(combos), len(beh_map)))
    for n, v in sorted(beh_map.items()):
        print("    %-16s %s" % (n, "%r" % (v[1],) if v[0] == "glyph" else "%s / %s" % (v[1], v[2])))
    if unbound:
        print("  unbound on every layer: %s"
              % ", ".join(str(i) for i in sorted(unbound)))


if __name__ == "__main__":
    main()
