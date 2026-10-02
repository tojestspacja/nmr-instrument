// The NMR numbers every model in this folder shares, in one place, with where each comes from.
// Included by housing.scad (the B0/f scale on the cover), probe.scad (the B0 pair) and
// instrument.scad (the checks across the whole setup). Change a default here, not in a model.

// firmware / host defaults (host/instrument.py NMR_DEFAULTS, firmware/NMR-FIRMWARE.md)
nmr_f_tx  = 89400;               // Hz, transmit = expected proton line
nmr_f_lo  = 84000;               // Hz, local oscillator
nmr_if    = nmr_f_tx - nmr_f_lo; // Hz, 5.4 kHz IF
nmr_gamma = 42.577e6;            // Hz/T, 1H gyromagnetic ratio / 2 pi
nmr_B0    = nmr_f_tx / nmr_gamma;   // T, 2.1 mT

// the class coil (workbook blocks/a.md, b.md; sheet_nmr_rx.py)
coil_L    = 2.53e-3;             // H
coil_R    = 6.7;                 // ohm DC
coil_Q    = 10;
tank_C    = 1 / (pow(2 * PI * nmr_f_tx, 2) * coil_L);   // F, about 1.25 nF
tank_fit  = [1.2e-9, 100e-12, 22e-12];                  // C710, C711, C712 on the board

// the board's limits on what drives the coils (design-decisions D-35, D-42; bring-up T-17, T-21)
vext_max  = 18;                  // V, +VEXT
hb_trip   = 2.0;                 // A, DRV8871 I_TRIP (B0 coil on J903 "H-BRIDGE")
tx_vpk    = 7.95;                // V, OPA564 output into the coil
rx_clamp  = 0.7;                 // V, D703/D704 crossed diodes at the RX SMA

// the acquisition (firmware/NMR-FIRMWARE.md settings defaults): what a record can contain
nmr_t90_us       = 417;          // us, 90 deg pulse
nmr_t_acq_start  = 1200e-6;      // s, end of the pulse to the first sample (dead time + margin)
nmr_t_acq        = 2.0;          // s, record length
nmr_rate         = 25000;        // S/s, decimated complex rate (100 kS/s / decim 4)
nmr_t_repeat     = 3.0;          // s, between scans

// the sample and the room (spectrum.py's simulator and instrument.scad's drift check)
water_T2   = 2.0;                // s, intrinsic T2 of tap water at low field (the line in a perfect field)
water_T1   = 2.5;                // s
cu_alpha   = 0.00393;            // 1/K, copper resistance tempco: a voltage-driven coil loses current as it warms
cu_c       = 385;                // J/(kg K), copper heat capacity
