#!/usr/bin/env python3
"""Keep the pinned ZMK display idle when this half has no display installed."""
from pathlib import Path
import sys
source = Path(sys.argv[1]).read_text()
for needle, replacement in [
    ('int zmk_display_init() {', 'extern bool ks_display_present(void);\nint zmk_display_init() {\n    if (!ks_display_present()) return 0;'),
    ('int display_event_handler(const zmk_event_t *eh) {', 'int display_event_handler(const zmk_event_t *eh) {\n    if (!ks_display_present()) return 0;'),
]:
    assert source.count(needle) == 1, 'Pinned ZMK display entry point changed'
    source = source.replace(needle, replacement)
# Include the blank/unblank workers, controlled by the saved runtime policy.
source=source.replace('#if IS_ENABLED(CONFIG_ZMK_DISPLAY_BLANK_ON_IDLE)', '#if 1 /* Studio runtime display policy */')
needle='    case ZMK_ACTIVITY_IDLE:\n'
assert source.count(needle)==1
source=source.replace(needle, needle+'        { extern bool ks_power_display_blank(void); if(!ks_power_display_blank()){start_display_updates();break;} }\n')
Path(sys.argv[2]).write_text(source)
