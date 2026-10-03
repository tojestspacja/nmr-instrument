/* Stable C ABI of the NMR core, used by Python (ctypes) and the browser (WebAssembly).
 * Conventions: SI units; arrays are caller-owned; functions return >= 0 on success, < 0 on error, and
 * nmr_last_error() describes the last failure. Not thread-safe (one simulator state per process/instance). */
#ifndef NMR_CAPI_H
#define NMR_CAPI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && !defined(__wasm__)
#define NMR_API __declspec(dllexport)
#elif defined(__wasm__)
#define NMR_API __attribute__((visibility("default")))
#else
#define NMR_API __attribute__((visibility("default")))
#endif

NMR_API const char* nmr_config_sha256(void);
/* struct sizes, so bindings can verify their layouts: timing, sequence, limits, model, record_meta, pipeline */
NMR_API int32_t nmr_abi_layout(int32_t* sizes6);
NMR_API const char* nmr_last_error(void);

/* ---- pulse programs */
typedef struct {
    double tick_hz, tx_freq_hz, pre_blank_s, dead_time_s, repetition_s;
} nmr_timing;
typedef struct {
    int32_t kind;          /* 0 FID, 1 Hahn echo, 2 inversion recovery, 3 CPMG */
    double t90, t180, tau, ti, acq_start, acq_len;
    int32_t n_echoes, n_avg, cyclops;
} nmr_sequence;
typedef struct {
    double tick_hz, tx_max_pulse_s, tx_max_duty, dead_time_s, pre_blank_s, tx_min_freq_hz, tx_max_freq_hz;
    int32_t has_sync_input;
} nmr_limits;

NMR_API void nmr_default_timing(nmr_timing* t);
NMR_API void nmr_default_sequence(nmr_sequence* s, int32_t kind);
NMR_API void nmr_default_limits(nmr_limits* l);
/* compile -> binary program (pulse/program.hpp format); returns the byte count or < 0 */
NMR_API int32_t nmr_compile(const nmr_timing* t, const nmr_sequence* s, uint8_t* out, int32_t cap);
/* number of rule violations (0 = valid), messages joined in nmr_last_error() */
NMR_API int32_t nmr_validate(const uint8_t* prog, int32_t n, const nmr_limits* l);
/* human-readable listing into out (NUL-terminated); returns length or < 0 */
NMR_API int32_t nmr_disassemble(const uint8_t* prog, int32_t n, char* out, int32_t cap);

/* ---- the instrument model and the simulator */
typedef struct {
    double gamma_bar, b0_current, t1, t2, proton_density, temperature;
    double earth_x, earth_y, earth_z;
    double tx_coil_current, tank_f0, tank_q, rx_gain, if_pole_hz, noise_density, lo_hz;
    double adc_full_scale, adc_rate, adc_t0, offset_i, offset_q;
    int32_t adc_bits, lo_coherent, interp;
    double beat_measurement_sigma;
    uint64_t seed;
    double iq_skew_samples;   /* I converted this many raw samples after Q (0 = ideal; hardware ~0.5, FW-IQ-001) */
} nmr_model;
NMR_API void nmr_default_model(nmr_model* m);

/* voxelise the design sample at grid pitch g [m] in the design probe's fields; returns the voxel count */
NMR_API int32_t nmr_voxelize(double grid_m);
/* copy voxel data: per voxel x,y,z [m], volume [m^3], |B0|/I [T/A], |B1perp|/I [T/A], phi [rad] -> 7 doubles */
NMR_API int32_t nmr_voxels(double* out7, int32_t cap_voxels);
/* field of the design coils per amp at a point: b0[3], b1[3] [T/A] */
NMR_API void nmr_fields_at(double x, double y, double z, double* b0, double* b1);
/* same, for the frozen 100 mm reference coil the golden maps were computed for (independent solver cross-check) */
NMR_API void nmr_legacy_fields_at(double x, double y, double z, double* b0, double* b1);

/* run a program on the current voxels; returns the number of acquisition records */
NMR_API int32_t nmr_simulate(const uint8_t* prog, int32_t n, const nmr_model* m);
typedef struct {
    uint32_t scan, window;
    double t_first, t_excitation, rx_phase_turns, beat_phase_true, beat_phase_measured, f_tx;
    int32_t n;
} nmr_record_meta;
NMR_API int32_t nmr_record(int32_t idx, nmr_record_meta* meta, int16_t* i, int16_t* q, int32_t cap);
NMR_API double nmr_sim_emf_peak(void);
NMR_API double nmr_sim_clip_fraction(void);

/* ---- processing of the current records (simulated, or loaded with nmr_load_record) */
NMR_API void nmr_clear_records(void);
NMR_API int32_t nmr_load_record(const nmr_record_meta* meta, const int16_t* i, const int16_t* q);
typedef struct {
    double adc_rate, adc_full_scale, fir_cutoff_hz, offset_tail, lo_hz;
    int32_t adc_bits, decimation, fir_taps, correct_beat, window_id;
    double iq_skew_samples;   /* I/Q aperture-skew correction: delay I this many raw samples onto Q's grid (FW-IQ-001) */
} nmr_pipeline;
NMR_API void nmr_default_pipeline(nmr_pipeline* p);
/* average the records of one window; writes interleaved re,im; returns samples, sets *fs and *t0 */
NMR_API int32_t nmr_process(const nmr_pipeline* p, double* out_reim, int32_t cap, double* fs, double* t0);
/* window: 0 rect, 1 hann, 2 exponential(tc), 3 half-hann. Returns bins; freq[] and interleaved value re,im */
NMR_API int32_t nmr_spectrum(const double* x_reim, int32_t n, double fs, int32_t window, double tc, int32_t pad,
                             double* freq, double* val_reim, int32_t cap);
NMR_API int32_t nmr_peak(const double* freq, const double* val_reim, int32_t n, double f_min, double f_max,
                         double guard, double* out4 /* freq, amplitude, noise_rms, snr */);
/* in-place complex FFT of n (power of two) interleaved values */
NMR_API int32_t nmr_fft(double* reim, int32_t n, int32_t inverse);

#ifdef __cplusplus
}
#endif
#endif
