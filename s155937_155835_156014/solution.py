import ctypes
import json
import os
import sys
import config

_DLL_PATH = os.path.join(os.path.dirname(__file__), "fast_arules.dll")
lib = ctypes.CDLL(_DLL_PATH)
lib.solve_arules.argtypes = [ctypes.c_char_p, ctypes.c_double, ctypes.c_double, ctypes.c_int]
lib.get_json_result.restype = ctypes.c_char_p
lib.get_elapsed_seconds.restype = ctypes.c_double

_OUT_FILE = getattr(config, 'output_path', 'rules.json')

def solve(min_support, min_confidence, verbose=False):
    lib.solve_arules(config.datapath.encode('utf-8'), min_support, min_confidence, 1 if verbose else 0)
    
    raw_json = lib.get_json_result()
    rules = json.loads(raw_json.decode('utf-8'))
    elapsed_ms = lib.get_elapsed_seconds() * 1000
    lib.free_result()

    if verbose:
        print(f"  [Engine] C++ logic time: {elapsed_ms:.2f} ms")
    
    return rules

if __name__ == "__main__":
    print(f"Uruchamiam rozwiązanie dla: {config.datapath}")
    results = solve(config.min_support, config.min_confidence, verbose=True)
    
    with open(_OUT_FILE, 'w', encoding='utf-8') as f:
        json.dump(results, f, indent=2)
    print(f"Gotowe! Znaleziono {len(results)} reguł. Zapisano do {_OUT_FILE}")
