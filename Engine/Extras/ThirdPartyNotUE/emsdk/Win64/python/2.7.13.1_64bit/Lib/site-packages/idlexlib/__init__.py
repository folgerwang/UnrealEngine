
import sys

try:
    import idlelib
except ImportError:
    print("** IdleX can't import IDLE. Please install IDLE. **")
    sys.exit(1)

from .idlexMain import version as __version__
from .extensionManager import extensionManager
