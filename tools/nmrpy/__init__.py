"""Python access to the C++ NMR core through its C ABI (physics/include/nmr/capi.h). No physics is reimplemented here.

    from nmrpy import core
    core.voxelize(0.0025)
    prog = core.compile_sequence(kind=0)
    recs = core.simulate(prog)

Build the shared library first: cmake -S . -B build -G Ninja && cmake --build build
"""

from __future__ import annotations

import ctypes as C
import os
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]


class Timing(C.Structure):
    _fields_ = [(n, C.c_double) for n in ("tick_hz", "tx_freq_hz", "pre_blank_s", "dead_time_s", "repetition_s")]


class Sequence(C.Structure):
    _fields_ = [("kind", C.c_int32)] + [(n, C.c_double) for n in ("t90", "t180", "tau", "ti", "acq_start", "acq_len")] + \
               [(n, C.c_int32) for n in ("n_echoes", "n_avg", "cyclops")]


class Limits(C.Structure):
    _fields_ = [(n, C.c_double) for n in ("tick_hz", "tx_max_pulse_s", "tx_max_duty", "dead_time_s", "pre_blank_s",
                                          "tx_min_freq_hz", "tx_max_freq_hz")] + [("has_sync_input", C.c_int32)]


class Model(C.Structure):
    _fields_ = [(n, C.c_double) for n in ("gamma_bar", "b0_current", "t1", "t2", "proton_density", "temperature",
                                          "earth_x", "earth_y", "earth_z", "tx_coil_current", "tank_f0", "tank_q",
                                          "rx_gain", "if_pole_hz", "noise_density", "lo_hz", "adc_full_scale",
                                          "adc_rate", "adc_t0", "offset_i", "offset_q")] + \
               [(n, C.c_int32) for n in ("adc_bits", "lo_coherent", "interp")] + \
               [("beat_measurement_sigma", C.c_double), ("seed", C.c_uint64)]


class RecordMeta(C.Structure):
    _fields_ = [("scan", C.c_uint32), ("window", C.c_uint32)] + \
               [(n, C.c_double) for n in ("t_first", "t_excitation", "rx_phase_turns", "beat_phase_true",
                                          "beat_phase_measured", "f_tx")] + [("n", C.c_int32)]


class Pipeline(C.Structure):
    _fields_ = [(n, C.c_double) for n in ("adc_rate", "adc_full_scale", "fir_cutoff_hz", "offset_tail", "lo_hz")] + \
               [(n, C.c_int32) for n in ("adc_bits", "decimation", "fir_taps", "correct_beat", "window_id")]


def _load():
    for name in ("libnmr.dll", "nmr.dll", "libnmr.so", "libnmr.dylib"):
        p = ROOT / "build" / name
        if p.exists():
            if hasattr(os, "add_dll_directory"):
                os.add_dll_directory(str(p.parent))
            return C.CDLL(str(p))
    raise OSError("build the core first: cmake -S . -B build -G Ninja && cmake --build build")


class Core:
    def __init__(self):
        L = self.L = _load()
        L.nmr_config_sha256.restype = C.c_char_p
        L.nmr_last_error.restype = C.c_char_p
        L.nmr_sim_emf_peak.restype = C.c_double
        L.nmr_sim_clip_fraction.restype = C.c_double
        L.nmr_compile.argtypes = [C.POINTER(Timing), C.POINTER(Sequence), C.c_void_p, C.c_int32]
        L.nmr_validate.argtypes = [C.c_void_p, C.c_int32, C.POINTER(Limits)]
        L.nmr_disassemble.argtypes = [C.c_void_p, C.c_int32, C.c_char_p, C.c_int32]
        L.nmr_voxelize.argtypes = [C.c_double]
        L.nmr_voxels.argtypes = [C.c_void_p, C.c_int32]
        L.nmr_fields_at.argtypes = [C.c_double] * 3 + [C.c_void_p, C.c_void_p]
        L.nmr_simulate.argtypes = [C.c_void_p, C.c_int32, C.POINTER(Model)]
        L.nmr_record.argtypes = [C.c_int32, C.POINTER(RecordMeta), C.c_void_p, C.c_void_p, C.c_int32]
        L.nmr_load_record.argtypes = [C.POINTER(RecordMeta), C.c_void_p, C.c_void_p]
        L.nmr_process.argtypes = [C.POINTER(Pipeline), C.c_void_p, C.c_int32, C.POINTER(C.c_double), C.POINTER(C.c_double)]
        L.nmr_spectrum.argtypes = [C.c_void_p, C.c_int32, C.c_double, C.c_int32, C.c_double, C.c_int32, C.c_void_p, C.c_void_p, C.c_int32]
        L.nmr_peak.argtypes = [C.c_void_p, C.c_void_p, C.c_int32, C.c_double, C.c_double, C.c_double, C.c_void_p]
        L.nmr_fft.argtypes = [C.c_void_p, C.c_int32, C.c_int32]

    def err(self) -> str:
        return self.L.nmr_last_error().decode()

    def config_sha256(self) -> str:
        return self.L.nmr_config_sha256().decode()

    # ---- programs
    def default_timing(self) -> Timing:
        t = Timing(); self.L.nmr_default_timing(C.byref(t)); return t

    def default_sequence(self, kind: int) -> Sequence:
        s = Sequence(); self.L.nmr_default_sequence(C.byref(s), kind); return s

    def default_limits(self) -> Limits:
        lim = Limits(); self.L.nmr_default_limits(C.byref(lim)); return lim

    def compile_sequence(self, kind: int = 0, timing: Timing | None = None, **seq) -> bytes:
        s = self.default_sequence(kind)
        for k, v in seq.items():
            setattr(s, k, v)
        t = timing or self.default_timing()
        buf = (C.c_uint8 * (8 * 8192 + 64))()
        n = self.L.nmr_compile(C.byref(t), C.byref(s), buf, len(buf))
        if n < 0:
            raise ValueError(self.err())
        return bytes(buf[:n])

    def validate(self, prog: bytes, limits: Limits | None = None) -> list[str]:
        lim = limits or self.default_limits()
        n = self.L.nmr_validate(prog, len(prog), C.byref(lim))
        if n < 0:
            raise ValueError(self.err())
        return [x for x in self.err().splitlines() if x]

    def disassemble(self, prog: bytes) -> str:
        buf = C.create_string_buffer(1 << 20)
        if self.L.nmr_disassemble(prog, len(prog), buf, len(buf)) < 0:
            raise ValueError(self.err())
        return buf.value.decode()

    # ---- fields and voxels
    def fields_at(self, x, y, z):
        b0 = (C.c_double * 3)(); b1 = (C.c_double * 3)()
        self.L.nmr_fields_at(x, y, z, b0, b1)
        return np.array(b0[:]), np.array(b1[:])

    def voxelize(self, grid_m: float) -> np.ndarray:
        n = self.L.nmr_voxelize(grid_m)
        out = np.zeros((n, 7))
        self.L.nmr_voxels(out.ctypes.data, n)
        return out   # x, y, z, volume, |B0|/I, |B1perp|/I, phi

    # ---- simulation and processing
    def default_model(self) -> Model:
        m = Model(); self.L.nmr_default_model(C.byref(m)); return m

    def simulate(self, prog: bytes, model: Model | None = None) -> list[dict]:
        m = model or self.default_model()
        n = self.L.nmr_simulate(prog, len(prog), C.byref(m))
        if n < 0:
            raise ValueError(self.err())
        return [self.record(k) for k in range(n)]

    def record(self, k: int) -> dict:
        meta = RecordMeta()
        n = self.L.nmr_record(k, C.byref(meta), None, None, 0)
        i = np.zeros(n, np.int16); q = np.zeros(n, np.int16)
        self.L.nmr_record(k, C.byref(meta), i.ctypes.data, q.ctypes.data, n)
        d = {f: getattr(meta, f) for f, _ in RecordMeta._fields_}
        d["i"], d["q"] = i, q
        return d

    def load_records(self, recs: list[dict]) -> None:
        self.L.nmr_clear_records()
        for r in recs:
            meta = RecordMeta(**{f: r[f] for f, _ in RecordMeta._fields_ if f in r and f != "n"})
            meta.n = len(r["i"])
            i = np.ascontiguousarray(r["i"], np.int16); q = np.ascontiguousarray(r["q"], np.int16)
            self.L.nmr_load_record(C.byref(meta), i.ctypes.data, q.ctypes.data)

    def default_pipeline(self) -> Pipeline:
        p = Pipeline(); self.L.nmr_default_pipeline(C.byref(p)); return p

    def process(self, pipeline: Pipeline | None = None, window: int = 0):
        p = pipeline or self.default_pipeline()
        p.window_id = window
        cap = 1 << 22
        out = np.zeros(2 * cap)
        fs = C.c_double(); t0 = C.c_double()
        n = self.L.nmr_process(C.byref(p), out.ctypes.data, cap, C.byref(fs), C.byref(t0))
        if n < 0:
            raise ValueError(self.err())
        z = out[:2 * n:2] + 1j * out[1:2 * n:2]
        return z, fs.value, t0.value

    def spectrum(self, z: np.ndarray, fs: float, window: int = 0, tc: float = 0.0, pad: int = 2):
        x = np.empty(2 * len(z)); x[0::2] = z.real; x[1::2] = z.imag
        cap = 1 << 23
        f = np.zeros(cap); v = np.zeros(2 * cap)
        n = self.L.nmr_spectrum(x.ctypes.data, len(z), fs, window, tc, pad, f.ctypes.data, v.ctypes.data, cap)
        if n < 0:
            raise ValueError(self.err())
        return f[:n], v[:2 * n:2] + 1j * v[1:2 * n:2]

    def peak(self, f: np.ndarray, s: np.ndarray, f_min: float, f_max: float, guard: float) -> dict:
        v = np.empty(2 * len(s)); v[0::2] = s.real; v[1::2] = s.imag
        o = np.zeros(4)
        self.L.nmr_peak(np.ascontiguousarray(f).ctypes.data, v.ctypes.data, len(s), f_min, f_max, guard, o.ctypes.data)
        return dict(zip(("freq_hz", "amplitude", "noise_rms", "snr"), o))


core = Core()
