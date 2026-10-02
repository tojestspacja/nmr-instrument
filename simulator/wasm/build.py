"""Build the NMR core (physics/, pulse/, generated/) as a WebAssembly reactor module for the browser.

The browser runs the SAME C++ sources as the native tests and the Python tools, through the C ABI
(physics/include/nmr/capi.h). No physics is written in JavaScript.

    py simulator/wasm/build.py            # -> simulator/web/nmrcore.wasm

Toolchain: clang (LLVM 19) with a WASI sysroot (wasi-sdk 25: wasi-sysroot + libclang_rt.builtins-wasm32).
Set WASI_ROOT to the folder holding them (default C:/Users/Liang/tools/wasi); wasm-ld must be in WASI_ROOT/bin.
"""

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WASI = Path(os.environ.get("WASI_ROOT", "C:/Users/Liang/tools/wasi"))
SYSROOT = WASI / "wasi-sysroot-25.0"
BUILTINS = next(WASI.glob("libclang_rt.builtins-wasm32*/libclang_rt.builtins-wasm32.a"))
OUT = ROOT / "simulator" / "web" / "nmrcore.wasm"

SOURCES = ["pulse/src/program.cpp", "pulse/src/compiler.cpp", "physics/src/field.cpp", "physics/src/dsp.cpp",
           "physics/src/probe.cpp", "physics/src/sim.cpp", "physics/src/pipeline.cpp", "physics/src/capi.cpp"]
EXPORTS = ["malloc", "free", "nmr_config_sha256", "nmr_abi_layout", "nmr_last_error", "nmr_default_timing", "nmr_default_sequence",
           "nmr_default_limits", "nmr_compile", "nmr_validate", "nmr_disassemble", "nmr_default_model", "nmr_voxelize",
           "nmr_voxels", "nmr_fields_at", "nmr_simulate", "nmr_record", "nmr_sim_emf_peak", "nmr_sim_clip_fraction",
           "nmr_clear_records", "nmr_load_record", "nmr_default_pipeline", "nmr_process", "nmr_spectrum", "nmr_peak",
           "nmr_fft"]


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        print(" ".join(map(str, cmd)))
        print(r.stdout + r.stderr)
        raise SystemExit(r.returncode)


def main() -> int:
    # compile with clang, link with lld's wasm flavour directly (a copied wasm-ld cannot find its DLLs on MinGW)
    obj = ROOT / "build" / "wasm"
    obj.mkdir(parents=True, exist_ok=True)
    objs = []
    for s in SOURCES:
        o = obj / (Path(s).stem + ".o")
        run(["clang++", "--target=wasm32-wasip1", f"--sysroot={SYSROOT}", "-O2", "-std=c++20", "-fno-exceptions",
             "-ffunction-sections", "-fdata-sections",
             f"-I{ROOT / 'physics/include'}", f"-I{ROOT / 'pulse/include'}", f"-I{ROOT / 'generated'}",
             "-c", str(ROOT / s), "-o", str(o)])
        objs.append(str(o))
    lib = SYSROOT / "lib" / "wasm32-wasip1"
    run(["lld", "-flavor", "wasm", "--no-entry", "--gc-sections", "--strip-debug", str(lib / "crt1-reactor.o"), *objs,
         f"-L{lib}", "-lc++", "-lc++abi", "-lc", "-lm", str(BUILTINS), "--export=_initialize",
         *[f"--export={e}" for e in EXPORTS], "-o", str(OUT)])
    print(f"wrote {OUT.relative_to(ROOT)} ({OUT.stat().st_size / 1024:.0f} KiB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
