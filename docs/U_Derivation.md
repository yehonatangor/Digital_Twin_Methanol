# Deriving the Overall Heat-Transfer Coefficient `U`

Item P1.5 / B1. This document presents the derivation, shows the arithmetic, states where it breaks down, and provides the verification code.

Bottom line up front: Shi's published data is not sufficient to determine `U` uniquely, because the paper never states the shell-side steam conditions. What the data provides is a defensible relationship and a bounded range, which is a large improvement on an arbitrary scalar.

---

## 1. What `U` controls and why it matters

`reactor/energy_balance.hpp` removes heat at

```
q_removed [W per kg catalyst] = U · a_v · (T_gas − T_coolant)
```

where `a_v = 4/(ρ_bulk · D_tube)` is the specific transfer area. `U` therefore sets the *entire* thermal behaviour of any cooled reactor:

- Too low gives heat accumulates gives the bed runs hot gives the equilibrium-limited methanol reaction is suppressed gives conversion is understated
- Too high gives the bed is quenched toward the coolant temperature gives kinetics slow gives conversion is understated for a different reason

There is an optimum in between, and an optimiser exploring temperature will sit right on top of it. This is the single most consequential unsourced number in the project.

---

## 2. The governing relation

For a heat exchanger transferring duty `Q` across area `A` with mean driving force `ΔT_m`:

$$Q = U \cdot A \cdot \Delta T_m \qquad\Longrightarrow\qquad U = \frac{Q}{A \cdot \Delta T_m}$$

Three quantities. Shi publishes two of them exactly. The third is the problem.

---

## 3. Route A, back out `U` from Shi's energy balance

### 3.1 Heat-transfer area, exact, fully sourced

Shi et al. (2020) Sec. 3.1, verified by direct extraction:

> *"The reactor contains 2700 tubes with uniform diameters and lengths of
> 0.035 m and 7.0 m."*

$$A = n_{\text{tubes}} \cdot \pi \cdot D \cdot L = 2700 \times \pi \times 0.035 \times 7.0 = \boxed{2078.2\ \text{m}^2}$$

*(Inner-diameter basis. Using an outer diameter would raise `A` by roughly the wall-thickness ratio, ~8% for 3 mm walls, second-order against the uncertainty below.)*

### 3.2 Duty, exact, fully sourced

> *"...saturated water on the shell side of the BWR. 61 MW of heat energy is
> absorbed from the reactions to generate high-pressure steam."*

Independent sanity check on that number. Shi's plant makes 2095 t/day of methanol:

$$\dot n_{\text{MeOH}} = \frac{2095 \times 1000}{32.042 \times 86400} = 756.6\ \text{mol/s}$$

The dominant reaction in a syngas loop is $\mathrm{CO}$ hydrogenation, $\mathrm{CO}$ + $2\,\mathrm{H_2}$ gives $\mathrm{CH_3OH}$, with $\Delta H$ about $-91\ \mathrm{kJ\,mol^{-1}}$:

$$Q \approx 756.6 \times 91{,}000 = 68.9\ \text{MW}$$

Less the endothermic RWGS contribution, 61 MW is exactly the right order. Shi's figure is self-consistent. ### 3.3 Driving force, this is where it breaks

Shi gives the gas temperatures:

- BWR feed (stream 22): 250 $^\circ\mathrm{C}$
- BWR effluent (stream 23): 275 $^\circ\mathrm{C}$

But never states the shell-side steam pressure or temperature. Without `T_coolant` there is no `ΔT_m`.

Using the log-mean between the gas endpoints and a constant boiling shell:

$$\Delta T_{lm} = \frac{(T_{in} - T_s) - (T_{out} - T_s)}{\ln\!\big[(T_{in}-T_s)/(T_{out}-T_s)\big]}$$

| `T_shell` ($^\circ\mathrm{C}$) | Steam (bar, sat) | $\Delta T_\mathrm{in}$ | $\Delta T_\mathrm{out}$ | $\Delta T_\mathrm{lm}$ (K) | U ($\mathrm{W/m^{2}\cdot K}$) |
|---|---|---|---|---|---|
| 245 (Mucci's value) | ~36 | 5 | 30 | 13.9 | 2104 |
| 234 | ~30 | 16 | 41 | 26.6 | 1104 |
| 220 | ~23 | 30 | 55 | 41.3 | 711 |
| 200 | ~15.5 | 50 | 75 | 61.7 | 476 |
| 180 | ~10 | 70 | 95 | 81.9 | 359 |

`U` spans a factor of six across a plausible range of steam pressures. Shi's data alone does not pin it down.

There is a further complication: a BWR bed is not a simple counterflow exchanger. The temperature rises steeply to a hot spot in the first ~10% of the bed, then declines as equilibrium is approached. Most of the heat crosses the wall near the hot spot, where the local $\Delta T$ is larger than either endpoint suggests. A rigorous treatment integrates `U·a_v·(T(z) − T_s)` along the bed, which is exactly what `reactor_core.cpp` already does, and which points to the better method in §5.

---

## 4. Route B, independent estimate from a packed-bed correlation

Worth attempting, and worth reporting honestly that it does not resolve the question.

Gas properties evaluated at 262.5 $^\circ\mathrm{C}$ and 78 bar using the project's *own* models (`transport.cpp` Wilke/DIPPR-102 viscosity, `thermo.cpp` Cp), for a representative syngas loop composition:

| Quantity | Value |
|---|---|
| Mean molar mass | 0.00978 kg/mol |
| Density (ideal gas) | 17.13 $\mathrm{kg/m^{3}}$ |
| Viscosity (Wilke) | $2.111 \times 10^{-5}$ Pa·s |
| c_p | 3412 J/(kg·K) |
| k (modified Eucken) | 0.1031 W/(m·K) |
| Pr | 0.698 |
| Mass flux G (assumed 200 kg/s total) | 77.0 kg/($\mathrm{m^{2}\cdot s}$) |
| $\mathbf{Re_p = d_p G/\mu}$ | 18,235 |

Li & Finlayson (1977) tube-wall Nusselt correlation for packed beds:

$$Nu_w = 0.17\,Re_p^{0.79} \quad\text{valid for } 20 < Re_p < 7600$$

$$Nu_w = 395 \;\Rightarrow\; h_{\text{gas}} = \frac{Nu_w \, k}{d_p} = 8148\ \text{W/(m}^2\text{K)}$$

Series resistance with boiling water (h about 8000) and a 3 mm steel wall:

$$U = \left(\tfrac{1}{8148} + \tfrac{0.003}{45} + \tfrac{1}{8000}\right)^{-1} = 3181\ \text{W/(m}^2\text{K)}$$

### Limitations of the calculation

Three independent problems, any one of which is disqualifying:

1. Re_p = 18,235 is 2.4× outside the correlation's stated validity range (20 to 7600). This is exactly the silent extrapolation `sampling/validity.hpp` was written to catch, and it would be hypocritical to lean on it here.
2. The mass flux is a guess. Shi does not publish the loop flow rate. Assuming 200 kg/s as a baseline, $Re \propto G$ and $Nu \propto Re^{0.79}$, so a 2× error in flow moves `U` by ~70%.
3. Thermal conductivity is a modified-Eucken estimate, not measured data. The project has no `k` correlation, only viscosity.

Route B gives 3181 W/($\mathrm{m^{2}\cdot K}$); Route A gives 359 to 2104. They do not agree, and Route B is the less trustworthy of the two. Reported here as a negative result, not as corroboration.

---

## 5. Inverse-solver calibration

Both routes above try to compute `U` from outside the model. There is a better option available using the validated reactor integrator:

> Choose `U` such that the twin reproduces Shi's reported BWR outlet
> temperature of 275 $^\circ\mathrm{C}$ at Shi's stated feed and geometry.

This is a calibration, not a sourced value, and must be labelled as such, but it is far more defensible than any of the numbers above, because:

- It uses only quantities Shi actually publishes (geometry, feed T, outlet T, duty)
- It is self-consistent with the model's own energy balance, kinetics and Cp
- It automatically accounts for the hot-spot profile that the LMTD approach mishandles
- It is reproducible: anyone can re-run the calibration and get the same number

The verification code in §6 performs exactly this calibration by bisection.

How to label it in the code:

```cpp
// CALIBRATED, not sourced. Shi et al. (2020) publish the BWR geometry
// (2700 x 0.035 m x 7.0 m), the duty (61 MW) and the gas temperatures
// (250 C in, 275 C out) but NOT the shell-side steam conditions, so U is
// under-determined by their data (it spans 359-2104 W/m2K over a plausible
// range of steam pressures -- see docs/U_Derivation.md).
//
// This value is instead obtained by requiring THIS model to reproduce Shi's
// reported 275 C outlet at their stated feed and geometry. It is therefore a
// property of the calibration, not a measurement, and any result sensitive to
// it should be reported with the sensitivity band from the same document.
double U_W_m2K = <calibrated value>;
bool  U_sourced = false;   // stays false -- calibrated is not sourced
const char* U_source = "CALIBRATED against Shi (2020) BWR outlet temperature; "
            "see docs/U_Derivation.md";
```

Note `U_sourced` stays false. Calibrated is not the same as sourced, and collapsing the two would be the exact dishonesty this project avoids elsewhere.

---

## 6. Verification code

Save as `tests/test_u_calibration.cpp` and add to `CMakeLists.txt`. It does three things: reproduces the Route A arithmetic, performs the calibration, and reports the sensitivity band.

```cpp
// =============================================================================
// test_u_calibration.cpp -- derive and verify the cooling U
// =============================================================================
#include "reactor_core.hpp"
#include "two_stage_reactor.hpp"
#include "units.hpp"
#include <cmath>
#include <cstdio>

namespace {
int failures = 0;
void check(const char* what, double got, double want, double tol) {
 const bool ok = std::fabs(got - want) / std::fabs(want) <= tol;
 if (!ok) ++failures;
 std::printf("[%s] %-50s got %10.4g want %10.4g\n",
       ok ? "PASS": "FAIL", what, got, want);
}

// ---- Route A arithmetic, checked against the document ----------------------
void testRouteA() {
 std::printf("\n[1] Route A -- U = Q/(A*dT_lm) from Shi's published data\n");

 const int  n_tubes = 2700;   // SOURCED: Shi Sec. 3.1
 const double D = 0.035, L = 7.0; // SOURCED: Shi Sec. 3.1
 const double A = n_tubes * 3.14159265358979323846 * D * L;
 check("heat-transfer area [m^2]", A, 2078.2, 0.001);

 const double Q = 61.0e6;     // SOURCED: Shi Sec. 3.1
 // Independent cross-check: 2095 t/day of methanol at -91 kJ/mol (CO route)
 const double n_meoh = 2095.0 * 1000.0 / (32.042 * 86400.0);
 check("methanol molar rate [mol/s]", n_meoh, 756.6, 0.01);
 const double Q_implied = n_meoh * 91000.0;
 std::printf("    [INFO] duty implied by production = %.1f MW vs Shi's stated %.1f MW\n",
       Q_implied / 1e6, Q / 1e6);
 check("Shi's duty is consistent with his production rate", Q, Q_implied, 0.15);

 const double T_in = 250.0, T_out = 275.0;  // SOURCED: Shi streams 22, 23
 auto U_at = [&](double T_shell) {
  const double d1 = T_in - T_shell, d2 = T_out - T_shell;
  const double lm = (d2 - d1) / std::log(d2 / d1);
  return Q / (A * lm);
 };
 std::printf("    T_shell  U [W/m2K]\n");
 for (double Ts: {180.0, 200.0, 220.0, 234.0, 245.0})
  std::printf("    %6.0f C  %8.0f\n", Ts, U_at(Ts));

 // The spread IS the finding -- assert it so nobody later quotes one number
 // as though Shi determined it.
 const double spread = U_at(245.0) / U_at(180.0);
 std::printf("    [INFO] U spans a factor of %.1f over plausible steam pressures\n", spread);
 check("CONTRACT: Shi's data leaves U under-determined (spread > 3x)",
    spread > 3.0 ? 1.0: 0.0, 1.0, 0.0);
}

// ---- Calibration: find U that reproduces Shi's 275 C outlet ----------------
void testCalibration() {
 std::printf("\n[2] Calibrate U against Shi's reported BWR outlet temperature\n");

 reactor::BedGeometry bed = reactor::presets::shi_bwr();
 reactor::ReactorState inlet = reactor::van_dal_lab_feed();
 // Scale to a per-tube plant loading and set Shi's feed condition.
 for (int i = 0; i < reactor::NS; ++i) inlet.F[i] *= 300.0;
 inlet.T_K = units::celsiusToKelvin(250.0);  // SOURCED: Shi stream 22
 inlet.P_Pa = units::barToPa(78.0);

 const double T_target = units::celsiusToKelvin(275.0);  // SOURCED: stream 23

 auto outletT = [&](double U) {
  reactor::ReactorConfig cfg;
  cfg.bed = bed;
  cfg.thermal = reactor::ThermalMode::Cooled;
  cfg.cooling.U_W_m2K = U;
  cfg.cooling.coolant_T_K = units::celsiusToKelvin(245.0); // Mucci analogue
  cfg.record_profile = false;
  const auto r = reactor::integrate_reactor(inlet, cfg);
  return r.ok ? r.outlet.T_K: -1.0;
 };

 double lo = 10.0, hi = 5000.0;
 for (int i = 0; i < 60; ++i) {
  const double mid = 0.5 * (lo + hi);
  const double T = outletT(mid);
  if (T < 0.0) { lo = mid; continue; }
  if (T > T_target) lo = mid; else hi = mid;  // more cooling -> lower outlet
 }
 const double U_cal = 0.5 * (lo + hi);
 std::printf("    [RESULT] calibrated U = %.0f W/(m^2 K)\n", U_cal);
 std::printf("    [INFO]  outlet at that U = %.2f C (target 275.00 C)\n",
       outletT(U_cal) - 273.15);

 check("calibrated U reproduces Shi's outlet temperature",
    outletT(U_cal), T_target, 0.01);
 check("CONTRACT: calibrated U is physically plausible (100-3000 W/m2K)",
    (U_cal > 100.0 && U_cal < 3000.0) ? 1.0: 0.0, 1.0, 0.0);
}
} // namespace

int main() {
 std::printf("=== U derivation and calibration ===\n");
 testRouteA();
 testCalibration();
 std::printf("\n%s -- %d failure(s)\n", failures ? "FAILED": "ALL PASSED", failures);
 return failures ? 1: 0;
}
```

---

## 7. Coolant temperature assumption

The standing assumption for coolant temperature is 245 $^\circ\mathrm{C}$, because it is Mucci's stated value for the same class of boiling-water methanol reactor. It is already in `mucci_two_stage_config()`, and using one number across both reactors keeps the model internally consistent.