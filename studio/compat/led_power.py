"""Keep automatic LED-rail transitions volatile in Studio's private build copy."""
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text()
# Studio persists the user's lighting policy separately. Layer changes and idle
# must not queue settings writes merely because the physical supply changes.
assert s.count('return ext_power_save_state();')==2
s=s.replace('return ext_power_save_state();','return 0; /* Studio owns persistent lighting policy. */')
Path(sys.argv[2]).write_text(s)
