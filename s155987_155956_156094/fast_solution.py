import sys
import os
import config

_build_dir = os.path.join(os.path.dirname(__file__), "build", "lib.win-amd64-cpython-311")
if os.path.isdir(_build_dir) and _build_dir not in sys.path:
    sys.path.insert(0, _build_dir)

try:
    import assoc_rules_engine as _engine
except ImportError as e:
    raise ImportError(
        "C++ module not built. Run:  pip install -e .\n"
        f"Original error: {e}"
    )


def solve(min_support, min_confidence, verbose=False):
    return _engine.find_rules(config.datapath, min_support, min_confidence, verbose)
