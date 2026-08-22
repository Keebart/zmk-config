# Repository instructions

## Sofle keymap matrix formatting

After every modification to `config/sofle_choc_pro.keymap`, format every
active and commented-out `bindings = < ... >` block according to the
physical Sofle matrix described below.

### Logical matrix

Treat every layer as 14 logical columns:

- Rows 1–3 contain 12 bindings:
  - columns 1–6: left half
  - columns 7–8: empty
  - columns 9–14: right half
- Row 4 contains 14 bindings:
  - columns 1–6: left half
  - columns 7–8: inner keys
  - columns 9–14: right half
- The thumb row contains 10 bindings:
  - columns 1–2: empty
  - columns 3–7: left thumb keys
  - columns 8–12: right thumb keys
  - columns 13–14: empty

Visual representation:

```text
01 02 03 04 05 06       09 10 11 12 13 14
01 02 03 04 05 06       09 10 11 12 13 14
01 02 03 04 05 06       09 10 11 12 13 14
01 02 03 04 05 06 07 08 09 10 11 12 13 14
      03 04 05 06 07 08 09 10 11 12
```

### Alignment rules

- Format each layer independently.
- Every `&` assigned to the same logical column must start at exactly the
  same character position within that layer.
- A complete ZMK binding, including its parameters and nested expressions,
  is one indivisible matrix cell.
- Determine each logical column's width from the longest binding occupying
  that column, followed by at least two spaces.
- Empty logical columns must still occupy their calculated horizontal space.
- Use spaces only, never tabs.
- Preserve the existing five-row structure.
- Do not use another layer as a formatting template.
- Never change bindings, arguments, ordering, comments, or behavior merely
  to achieve alignment.
- After editing, run `git diff --check`.
