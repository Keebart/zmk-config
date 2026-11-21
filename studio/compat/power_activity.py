"""Adapt pinned activity timeouts for Studio without altering upstream sources."""
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text()
for old,new in [('#define MAX_IDLE_MS CONFIG_ZMK_IDLE_TIMEOUT','extern uint32_t ks_power_idle_ms(void);\nextern uint32_t ks_power_sleep_ms(void);\n#define MAX_IDLE_MS ks_power_idle_ms()'),('#define MAX_SLEEP_MS CONFIG_ZMK_IDLE_SLEEP_TIMEOUT','#define MAX_SLEEP_MS ks_power_sleep_ms()'),('int32_t inactive_time = current - activity_last_uptime;','uint32_t inactive_time = (uint32_t)current - activity_last_uptime;')]:
    assert s.count(old)==1,old
    s=s.replace(old,new)
needle = '            set_state(ZMK_ACTIVITY_IDLE);\n        }'
assert s.count(needle) == 1
s=s.replace(needle, needle+' else {\n            set_state(ZMK_ACTIVITY_ACTIVE);\n        }')
Path(sys.argv[2]).write_text(s)
