# rpscprog
### A tool to expedite programming/configuration of controllers at scale

## Installation / Usage
```bash
python3.9 -m venv venv
. ./venv/bin/activate
pip install ./
rpscprog --help
# Usage: rpscprog [OPTIONS] COMMAND [ARGS]...
# 
# Options:
#   -p, --port PATH     [required]
#   -b, --baud INTEGER  [default: 115200]
#   --verbose
#   --debug
#   --help              Show this message and exit.
# 
# Commands:
#   enter-prog  Enters programming mode on the target board
#   exit-prog   Exits programming mode on all connected boards currently in
#               programming mode
#   set-option  Sets the specified configuration parameter to the provided value
#   test        Tests all motors in the -ve direction for 2 seconds each in
#               logical/channel order
```
