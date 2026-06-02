# zmk-config

Personal [ZMK](https://zmk.dev) firmware for several wireless split keyboards —
**Piantor Pro BT**, Corne Choc Pro, and Sofle Choc Pro.

The **Piantor Pro BT** is the actively-tuned board and is documented below: a 42-key
(3×6 + 3 thumbs/hand) low-profile split with nice!view displays, nRF52840 controllers,
and a layout ported from a ZSA Voyager (Oryx) config.

## Piantor Pro BT

Home-row mods, 5 active layers. RGB underglow is disabled to save battery; the active
layer name shows on the nice!view.

### Layers & how to reach them

| # | Layer | Reached by |
|---|---------|-----------|
| 0 | **QWERTY** | base |
| 1 | **NUM/SYM** | hold `SPACE` (left thumb) |
| 2 | RSVD | unused / reserved |
| 3 | **APPS** | hold far-right thumb |
| 4 | **NAV** | hold `⌘` thumb (left-inner) |
| 5 | **SYSTEM** | hold far-left thumb |

`·` = transparent/empty. Thumb keys are shown on the short bottom row.

### 0 · QWERTY

Home-row mods: **tap** the key for the letter, **hold** for the modifier shown beneath it.
Mods are bilateral — a hold only registers when the next key is on the *opposite* hand.

```
TAB   Q    W    E    R    T   │   Y    U    I    O    P    \
CAPS  A    S    D    F    G   │   H    J    K    L    ;    '
      ⌃    ⌥    ⇧    ⌘        │        ⌘    ⇧    ⌥    ⌃
ESC   Z    X    C    V    B   │   N    M    ,    .    /    ⌥⌘⇧C / ⌃↓
            SYS  SPC  ⌘       │   RET  BSPC APP
```

- Bottom-right corner: **tap** `⌥⌘⇧C`, **hold** `⌃↓`.
- Thumbs (L→R): `SYS` = hold→layer 5 · `SPACE` = tap space / hold→layer 1 · `⌘` = tap ⌘ / hold→layer 4 (NAV) · `RET` · `BSPC` · `APP` = hold→layer 3.

### 1 · NUM/SYM

```
`    !    @    #    $    %    │   7    8    9    -    /    ·
~    ^    &    *    (    )    │   4    5    6    +    *    ·
–/—  -    [    ]    {    }    │   1    2    3    .    =    ·
            ·    ·    ·       │   0    ·    ·
```

- Bottom-left corner: `–/—` = **tap** en-dash (`⌥-`) / **hold** em-dash (`⌥⇧-`); `-` to its right = hyphen.

### 3 · APPS

Every letter sends **Hyper** (`⌘⌃⌥⇧`) + that letter, in QWERTY positions — bind these to
app-launch / window shortcuts on macOS.

```
·   Hyper+Q  W   E   R   T   │   Y   U   I   O   P   ·
·   Hyper+A  S   D   F   G   │   H   J   K   L   ·   ·
·   Hyper+Z  X   C   V   B   │   N   M   ·   ·   ·   ·
              ·    ·    ·    │   ·    ·    ·
```

### 4 · NAV

```
·    ·    ·    ·    ·    ·    │   ·     ·     ·     ·     ·     ·
·    ←    ↓    ↑    →   LMB   │  LMB    ⌘     ⇧     ⌥     ⌃     ·
·    ·    ·   Scrl↑ Scrl↓ ·   │   ·   Scrl↑ Scrl↓   ·     ·     ·
            ·    ·   base     │   ·     ·     ·
```

- `LMB` = left mouse click · arrows + scroll wheels · right home row = bare mods to hold while mousing · left thumb `base` = jump to layer 0.

### 5 · SYSTEM

```
BT0  BT1  BT2  BT3  BT4  BT⌫  │   ·     ·     ·     ·     ·    🔒
BOOT RST  UNLK  V-   V+  MUTE │  ⌃⌥←   ⌃⌥↓   ⌃⌥↑   ⌃⌥→   ⌃⌥⏎    ·
·    ·    ⌘⇧5  ⌘⇧4 ⌥⌘⇧/  ·   │  ⌃⌥U   ⌃⌥I   ⌃⌥J   ⌃⌥K    ·     ·
            ·    ·    ·       │   ·     ·     ·
```

- `BT0–BT4` = select Bluetooth profile · `BT⌫` = clear profile · `🔒` = lock screen (`⌘⌃Q`).
- `BOOT` = bootloader · `RST` = reset · `UNLK` = ZMK Studio unlock.
- Volume on `D`/`F`/`G`. `⌥⌘⇧/` = mac shortcut · `⌘⇧4`/`⌘⇧5` = screenshots · `⌃⌥`-arrows / `⌃⌥`-U/I/J/K = window & desktop macros.

## Building & flashing

Firmware builds in GitHub Actions (**Build ZMK firmware**). The `.uf2` artifacts are on
each run; download the `nice_view-piantor_pro_bt_left/right-zmk.uf2` pair.

To flash a half: plug in via USB-C, **double-tap reset** to mount the `KEEBART` drive,
drag the matching left/right `.uf2` onto it, and wait ~20 s for it to reboot. A
"could not copy extended attributes" / "disk not ejected" warning is normal — it just
means the board rebooted. Flash both halves with their respective files.

If the halves stop pairing after a firmware change, flash the `settings_reset` build to
both halves, then re-flash the normal firmware.
