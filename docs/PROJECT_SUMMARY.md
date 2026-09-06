# methanol_twin: Project Summary for External Review

A zero-dependency C++17 simulation engine for reacting gas-recycle flowsheets, applied to direct $\mathrm{CO_2}$ hydrogenation to methanol.

This is a computational methods and physical modelling project, not a techno-economic plant design. The contribution is the numerical core and what it reveals about closed recycle loops:

1. A zero-dependency C++17 engine for reacting gas-recycle flowsheets, with mass-based RK4 integration, multi-regime phase equilibrium (Peng-Robinson 1978 and NRTL) and coupled Ergun bed hydraulics.
2. Analysis of the loop accumulation trap and of the stoichiometric feed ceiling at R/3.0 in closed direct-$\mathrm{CO_2}$ loops, and the demonstration that the first is a hydraulic cost rather than an infeasibility once the reactor's thermal boundary condition is set correctly.
3. A quantified Pareto front between carbon yield and loop hydraulics, and the finding that it is not monotone: at intermediate feed ratios a tighter purge raises yield and lowers compression duty together. The stoichiometric feed ratio is dominated, costing 14 MW of circulator duty for 0.85 points of yield. Tolerance to catalyst ageing moves along the same axis, which a study optimising on yield alone would not see.
4. Resolution of low-purge convergence limits as Ergun bed momentum loss rather than solver instability, together with the demonstration that Wegstein acceleration is load-bearing instead of convenient at high recycle ratios: plain successive substitution does not converge at the design point.
5. Thermodynamic and elemental conservation to machine precision across phase and reaction splits.

Scope is the whole tree. The physical and unit-operation core is documents 01 to 19; equipment costing, plant economics, storage and dispatch, the flowsheet layer and the sampling provenance modules are documents 20 to 25. Every module now ships with the documentation describing it.

This document is written for an independent reviewer. It describes what the model does, how it is put together, what has been verified and against what, and what is known to be weak. The last section lists the questions where a second opinion would be most useful.

---

## 1. What the model is for

The plant takes carbon dioxide and electrolytic hydrogen and makes methanol. Two feed routes are modelled: direct $\mathrm{CO_2}$ hydrogenation, and a tri-reforming front end that converts natural gas, $\mathrm{CO_2}$, steam and oxygen into synthesis gas.

The intended use is steady-state design and optimisation, specifically parameter sweeps and training data for machine-learning surrogates. It is not a dynamic simulator. There are no compressor performance curves, no controller models, and no time-domain response. That is a deliberate scope boundary, not an omission, and it is the reason certain reviewer suggestions in section 8 are listed as out of scope instead of deferred.

### The governing rule

Every numeric constant is one of three things, and which one is always stated at the point of use:

1. Sourced. Traceable to a specific table, equation or passage in a named publication.
2. Derived. Computed from sourced values by a derivation written out in the documentation, and flagged `*_sourced = false` because derived is not the same as sourced.
3. Placeholder. Not available from any source in the project's library, marked as such in the code, and accompanied by a statement of what would close it.

There are no unlabelled magic numbers. Section 5 lists every item in categories 2 and 3 so a reviewer can go straight to the weak points.

---

## 2. Architecture

![Module architecture, the seven layers of the physical core](figures/02-architecture-layers.svg)

Seven layers. Each compiles against the layers above it and nothing below, which is enforced by building a layer in isolation from its own files plus the earlier layers only. That check passes for layers 1 to 5; layers 6 and 7 are verified as part of the whole-library build.

| Layer | Modules | What it establishes |
|---|---|---|
| 1 Foundation | `units`, `species`, `stream`, `thermo` | 9 species, DIPPR-107 heat capacity, DIPPR-101 vapour pressure, formation enthalpy and entropy, Kirchhoff integration, equilibrium constants from Gibbs energy |
| 2 Phase equilibrium | `eos`, `nrtl`, `flash` | Peng-Robinson with both the 1976 and 1978 $\kappa$ branches, NRTL activity coefficients, Rachford-Rice flash with a regime split at 10 bar |
| 3 Reaction kinetics | `lhhw`, `trm_kinetics` | Vanden Bussche and Froment rate form for methanol synthesis, steam and dry reforming equilibria |
| 4 Packed-bed reactor | `transport`, `ergun`, `energy_balance`, `reactor_core`, `two_stage_reactor` | Wilke viscosity mixing, Ergun pressure drop in mass-flux form, energy balance, RK4 over catalyst mass on 11 coupled equations |
| 5 Tri-reforming | `trm_reactor` | Xu and Froment reactions I, II and III plus Trimm and Lam methane combustion, with Aboosadi's parameters and effectiveness factors |
| 6 Process units | `electrolyzer`, `compressor`, `compressor_train`, `heat_exchanger`, `knockout_drum`, `recycle`, `membrane`, `distillation`, `feed_blend` | The unit-operation library the flowsheets compose |
| 7 Deactivation | `activity_decay`, `aged_reactor` | Catalyst activity decay and its coupling into the reactor |

### Two structural decisions worth understanding

Molar flow is the single source of truth. A `Stream` carries molar flows per species plus temperature and pressure. Mass flows, mole fractions and mass fractions are all computed on demand. Nothing stores a derived quantity that could drift out of step with the flows.

Composition is per tube inside the reactor, plant-scale at its boundary. `reactor_core` integrates one tube. The flowsheet divides by the tube count on the way in and multiplies on the way out. Getting this wrong would scale conversion, so the boundary is explicit in the code and asserted in the tests.

---

## 3. The physical model, layer by layer

### Thermodynamics (layer 1)

Heat capacity uses the DIPPR-107 hyperbolic form. Vapour pressure uses DIPPR-101. Reaction enthalpies come from formation data integrated with Kirchhoff's relation, so there is exactly one source of thermochemical truth and everything else derives from it. Hydrogen's lower heating value, for instance, is computed from the combustion reaction instead of carried as a separate tabulated constant, which is why it reproduces the handbook 119.96 MJ/kg to five figures without that number appearing anywhere in the code.

### Phase equilibrium (layer 2)

Peng-Robinson implements both acentric-factor branches. This matters: methanol's $\omega$ is 0.566, above the 0.49 threshold, so the 1976 branch would misprice it. The cubic is solved by Cardano's method with explicit vapour and liquid root selection.

NRTL binary parameters come from the ChemSep databank, converted from cal/mol by dividing by R = 1.987204. Pairs without fitted parameters are reported as opposed to treated as ideal, and the count of unparameterised pairs is a field on the flash result.

The flash chooses between a fugacity-fugacity and an activity-fugacity formulation at 10 bar. High-pressure gas loops take the equation of state; low-pressure polar liquid mixtures take the activity model. This is the split a process engineer would specify by hand, applied automatically.

### Kinetics (layer 3)

The methanol synthesis rate is the Vanden Bussche and Froment LHHW form with Mignard and Pritchard's reparameterisation, constants per Van-Dal Table 3.

One correction is worth flagging to a reviewer because it looks like an error until it is explained. Van-Dal's Eq. (9) prints the water-gas shift equilibrium constant with both signs inverted. The form implemented is Graaf's original, $\log_{10} K$ = 2073/T − 2.029, which is the only version that makes the reverse-shift driving force vanish at equilibrium. The evidence is written out in doc 08 and the corrected behaviour is pinned by three assertions in `test_kinetics.cpp`.

### Reactor (layer 4)

Eleven coupled equations (nine species, temperature, pressure) integrated by fixed-step RK4 over catalyst mass instead of length, which makes the pressure drop term dimensionally natural.

Two design choices enforce numerical stability:

Step size, not step count. The grid is specified by maximum catalyst mass per step. A step *count* tuned on the 0.0348 kg laboratory bed becomes 206 times coarser on the 7.17 kg plant bed, which previously produced a 10 percent overstatement of methanol production that passed the laboratory validation gate cleanly. Grid independence is now demonstrated to $1 \times 10^{-8}$ across a 50-fold step refinement.

The non-negativity clamp is measured, not silent. RK4 can drive a nearly depleted species slightly negative. The clamp records the largest excursion as a fraction of inlet flow and aborts if it exceeds tolerance, so a result can never rest on fabricated moles. At the validation point the clamp absorbs exactly zero.

### Tri-reforming (layer 5)

Four reactions: steam reforming, water-gas shift, the overall reaction, and methane combustion. Rate constants verified inside Xu and Froment's own stated confidence intervals. Effectiveness factors from Aboosadi.

### A note on the recycle solver

The loop is torn on the recycle stream and solved by successive substitution with per-component Wegstein acceleration, q bounded to (−5, 0.95), which takes the design point from 355 passes to 28. Convergence failures below 10 percent purge are not solver failures: the messages are a right-hand-side failure or a pressure below the guard partway down the tube, which is the Ergun drop consuming the loop pressure. No accelerator fixes a bed that cannot pass the flow, which is why the fresh-ratio trim instead of a better solver is what moves that limit.

### Process units (layer 6)

The separations are the part most worth a reviewer's attention because they mix rigour levels deliberately. The knockout drum is real phase equilibrium through the validated flash. The membrane and the recycle splitter are performance specifications with sourced split fractions. The distillation column is a lumped recovery and purity model, because neither Shi nor Van-Dal publishes the tray-level data a rigorous column would need. Which is which is stated in doc 17 instead of left for the reader to infer.

### Deactivation (layer 7)

Fichtl's power-law decay with an Arrhenius rate constant, fitted through the two third-order points at 523 and 553 K. The 493 K point is exposed separately because Fichtl's fitted order there is 4, not 3, and folding it into a smooth curve would turn a real order change into apparent continuous behaviour.

`aged_reactor` couples activity into the reactor by scaling the rate. Activity
1.0 reproduces the fresh reactor bit for bit, which is asserted, because the aged integrator is a near-copy of the fresh one and any divergence would otherwise be silent.

---

## 3b. The canonical design point

All figures below are the converged default configuration, reproducible by running `src/main.cpp` with no arguments.

| Parameter | Value | Status |
|---|---|---|
| Catalyst mass | 44,500 kg | Sourced, Van-Dal Table 2, unscaled |
| Tubes | 6,204 | derived from the charge and Shi's tube geometry |
| Reactor model | multi-tubular BWR, cooled | jacket 245 $^\circ\mathrm{C}$, U = 980 $\mathrm{W\,m^{-2}\,K^{-1}}$, derived |
| Peak bed temperature | 250.9 $^\circ\mathrm{C}$ at 33 % of length | inside Fichtl's fitted 523 to 553 K band |
| Reactor outlet | 248.3 $^\circ\mathrm{C}$ | classic commercial declining profile |
| Fresh $\mathrm{H_2{:}CO_2}$ ratio, R | 2.95 | chosen at the knee, not sourced |
| Purge fraction | 1.0 % | Sourced, Van-Dal Sec. 2.3.2 |
| Reactor inlet SN | 8.55 | hydrogen-rich, as a near-stoichiometric feed implies |
| Inlet p($\mathrm{CO_2}$) | 6.09 bar | $\mathrm{CO_2}$-lean, the cost of recycling to extinction |
| Recycle ratio | 8.3 × fresh feed | |
| Overall $\mathrm{CO_2}$ conversion | 97.77 % | against Van-Dal's reported ~93 % |
| Per-pass $\mathrm{CO_2}$ conversion | 38.4 % | |
| Methanol product | 17.21 $\mathrm{kg\,s^{-1}}$ | distillate basis, after 99.5 % recovery |
| Carbon yield | 97.2 % | of the 17.80 $\mathrm{kg\,s^{-1}}$ ceiling, and 98.8 % of the R/3 cap |
| Circulator duty | 4.84 MW | |
| Feed pre-heat | 96.4 MW | fully covered by the feed-effluent exchanger |
| Fresh $\mathrm{H_2}$ lost to tail gas | 0.9 % | |
| Chemical energy to fuel | 1.3 % | |
| Catalyst activity floor | 0.45 | below this the loop stops converging |
| Wegstein passes | 115 | plain substitution does not converge here |

### Reproducing the reference plant on its published charge

Van-Dal reports about 93 % overall $\mathrm{CO_2}$ conversion on a 44,500 kg charge. This model reproduces that on the published charge with no scaling of the bed. At $R = 3.00$ the sweep gives 92.6 percent at an 8 percent purge and 93.4 percent at 7 percent, and conversion keeps rising as the purge closes: 96.9 percent at 3 percent and 98.8 percent at 1 percent. The canonical point, $R = 2.95$ at a 1 percent purge, reaches 97.8 percent.

That match should be read for what it is. The purge fraction is not published, so it is a one-parameter fit to a single reported number, not a blind prediction. What it establishes is that the published charge and the published conversion are mutually consistent under this model at a purge a real plant would run.

Solving an adiabatic bed cannot close the loop below a 10 % purge at R = 3.0. Switching the thermal boundary condition to the jacketed reactor every cited industrial converter actually uses removes this limitation.

The per-pass conversion of 50 % is not comparable to Van-Dal's reported 33 %: that figure is a laboratory measurement at 50 bar and 220 $^\circ\mathrm{C}$ on 34.8 g of catalyst. Overall conversion is the plant-boundary quantity, and it is the one compared here.

### The central trade-off: carbon yield against loop hydraulics

Carbon yield rises monotonically with the fresh ratio R across the whole operating grid. Trimming R does not buy yield; it buys a smaller loop. The two branches below are both design points on one Pareto front, not a correct and an incorrect setting.

![Operating envelope: carbon yield against loop compression duty](figures/03-operating-envelope.svg)

The trade is not monotone, which is the figure's main content. Along R = 2.85 and R = 2.95 a tighter purge raises yield and lowers circulator duty together, because higher conversion per pass shrinks the recycle faster than the tighter purge grows it. Only at R = 3.00 does the accumulating hydrogen surplus take over, and that curve then turns almost vertical: the last 0.05 of feed ratio costs 14 MW for 0.85 points of yield.

That is why the design point sits at R = 2.95 instead of at stoichiometry. An earlier revision of this project used R = 3.00 at a 3 % purge, and the sweep shows it was dominated on every axis at once.

The feasible region is also not convex: at R = 2.70 the loop converges at 3 % purge and at 1 % but fails at 2 %, and that failure is a genuine integration failure inside the bed instead of an iteration limit. Any optimiser or surrogate trained on this space has to be told so.

| | Canonical (R = 2.95, 1 % purge) | Stoichiometric (R = 3.00, 1 %) | Compact loop (R = 2.40, 5 %) |
|---|---|---|---|
| Carbon yield | 97.2 % | 98.0 % | 77.5 % |
| Overall conversion | 97.8 % | 98.8 % | 80.0 % |
| Yield ceiling, R/3 | 98.3 % | 100 % | 80.0 % |
| Recycle ratio | 8.3 × | 13.8 × | 5.0 × |
| Circulator duty | 4.84 MW | 18.86 MW | 1.30 MW |
| Feed pre-heat | 96.4 MW | 159.6 MW | 54.1 MW |
| Effluent cooling load | 156.6 MW | | 97.5 MW |
| Reactor inlet SN | 8.55 | 18.34 | 1.63 |
| Inlet p($\mathrm{CO_2}$) | 6.09 bar | 2.78 bar | 19.81 bar |
| Catalyst activity floor | 0.45 | 0.80 | 0.20 |

The compact branch cuts compression duty by 73 % against the canonical point, at the cost of 20 points of carbon yield. Its yield is capped at R/3 = 80.0 % by the feed itself, and at 77.5 % it is already 96.9 % of that cap, so no amount of additional catalyst can move it. The canonical point is capped at 98.3 % and reaches 98.8 % of that.

The stoichiometric column is the reason R = 2.95 instead of 3.00 is the default. Feeding the last 0.05 buys 0.85 points of carbon yield and costs 14 MW of circulator duty, a factor of four, while the ageing floor rises from 0.45 to
0.80. Stoichiometry is a presentational virtue, not a physical one.

The last row is the one a design study would miss. Lower catalyst activity means less conversion per pass, which means more recycle for the same duty, which means a larger Ergun drop. The canonical point runs an 8.3 times recycle when the catalyst is fresh and loses convergence at an activity of about 0.45; the stoichiometric case gives up at 0.80 and the compact branch is still solving at
0.20. Carbon yield, compression duty and tolerance to ageing are three faces of the same trade.

### Why the reactor is cooled

`ThermalMode` defaults to `Cooled` at the flowsheet layer. This is not a refinement, it is what makes the operating region above reachable at all: adiabatically the loop fails at a 10 % purge and below when fed at R = 3.0. Cooling holds the bed near the temperature at which the exothermic equilibrium is favourable, so a dilute hydrogen-rich inlet still reacts. Van-Dal's own laboratory validation case is a genuinely adiabatic fixed bed, so `reactor_core.hpp`'s own default stays `Adiabatic`; changing that would break the per-pass validation gate.

One consequence is reported instead of hidden. At the canonical point the peak bed temperature is 522.7 K, about half a kelvin below the lower edge of Fichtl's fitted 523 to 553 K deactivation band, so that correlation is extrapolated by a small margin and the validity reporting says so.

## 4. Verification

Independently re-run for this summary, from a clean build.

### Build and test

The whole tree is 48 translation units, 38 test binaries and 1004 assertions. The table below is the separable physical core, layers 1 to 7 only, which is reported separately because it is the part that compiles standalone.

| Metric | Result |
|---|---|
| Translation units, layers 1 to 7 | 24 |
| Headers | 26 |
| Lines of code | 5,770 |
| Compiler warnings under `-Wall -Wextra` | 0 |
| Test binaries | 16 |
| Assertions | 420 |
| Failures | 0 |
| Layers building in isolation | 1 to 5 verified |
| Forward dependencies from layers 1 to 7 into the flowsheet and economics layers | none |

### Reproduction of published values

| Claim | Computed | Published | Source |
|---|---|---|---|
| Per-pass $\mathrm{CO_2}$ conversion, laboratory bed | 33.84 % | 33 % | Van-Dal Table A.3 |
| Hydrogen LHV | 119.955 MJ/kg | 119.96 | derived from formation data |
| Water per kg $\mathrm{H_2}$ | 8.9360 | 8.937 | electrolysis stoichiometry |
| Oxygen per kg $\mathrm{H_2}$ | 7.9363 | 7.937 | electrolysis stoichiometry |
| Compressor at $\beta = 2.0$ | 709.9 kW | 710 | Mucci Table A.2 |
| Compressor at $\beta = 3.5$ | 1396.3 kW | 1397 | Mucci Table A.2 |
| Shift constant at 493.15 K | 149.48 | 130 to 150 | Graaf, via Van-Dal |
| Catalyst activity loss at 1600 h | 57.9 % | about 58 % | Fichtl |
| Methanol/water bubble curve | within 0.008 | 11 compositions | DECHEMA |
| Tri-reformer $\mathrm{CH_4}$ conversion | 97.6 % | high | Aboosadi |
| Tri-reformer $\mathrm{H_2/CO}$ | 1.797 | near 2 | methanol syngas target |

### Structural checks

| Check | Result |
|---|---|
| Carbon, hydrogen, oxygen balances across the reactor | $2 \times 10^{-16}$ relative |
| Non-negativity clamp at the validation point | 0 |
| Grid independence, 50-fold refinement | $1 \times 10^{-8}$ |
| Aged reactor at activity 1.0 versus fresh | bit-identical |
| Guards: temperature bound, empty bed, zero inlet temperature | all trip correctly |
| Rate laws reverse past equilibrium | asserted both directions |

The bubble-curve test deserves a note. NRTL is asymmetric, so swapping the two halves of the methanol/water pair produces a plausible-looking but wrong separation. `test_vle.cpp` exists specifically to make that failure loud, with tolerances set between the correct curve and the swapped one.

---

## 5. Known weaknesses, stated 

This is the section to read first if you are looking for problems.

### Placeholders: not sourced, and flagged in code

| Item | Location | What would close it |
|---|---|---|
| Tri-reformer bed void fraction 0.5 and particle density 1775 $\mathrm{kg/m^{3}}$ | `trm_reactor.cpp`, `kAboosadiBedGeometrySourced = false` | Bed geometry from Aboosadi or an equivalent tri-reformer paper. The density is borrowed from Van-Dal's methanol catalyst, which is a different material |
| Water acceleration of catalyst decay | `activity_decay.hpp`, `kFichtlWaterMultiplierSourced = false` | A fitted water-pressure exponent. Fichtl reports the effect qualitatively but publishes no correlation, and his one co-feed experiment ran at a different space velocity, so a two-point ratio would not isolate it. Defaults to 1.0, meaning dry-gas kinetics |
| Industrial decay activation energy | `activity_decay.hpp`, `kIndustrialEdSourced = false` | Hanken's MSc thesis. The preset works using a stated interim value; the thesis would upgrade the temperature dependence from interim to sourced |

### Derived, not sourced

| Item | Basis |
|---|---|
| Overall heat-transfer coefficient U | Derived under a stated assumption in `energy_balance.hpp`. `U_sourced = false`. The full derivation is in `docs/U_Derivation.md` |
| Compressor mechanical efficiency 0.877 | Back-solved as the mean of four points from Mucci Table A.2. Not printed in the paper. `eta_mechanical_sourced = false` |
| Interstage temperature, two-stage reactor | `interstage_T_sourced = false` |

### Substitutions

The plant-scale bed combines Van-Dal's Table 2 catalyst properties with Shi's Sec. 3.1 tube diameter and length, because Van-Dal publishes no tube geometry. The tube count is then derived from Van-Dal's own stated 44,500 kg charge as opposed to adopted from Shi. Adopting Shi's 2,700 tubes wholesale undersized the bed by a factor of 2.3 and was caught only when the recycle loop failed to close.

### The fresh hydrogen ratio, and a finding worth reviewing carefully

This is the most substantive result in the flowsheet layer. It was surfaced by external review, tested directly, and then reversed when the thermal boundary condition changed. Both stages are recorded here because the reversal is itself the finding.

The mechanism. The reverse shift consumes $\mathrm{CO_2}$ while taking only one $\mathrm{H_2}$, so the reactor's net consumption ratio is below 3.0. Measured here it is 2.695, which matches the closed form R = 3 − 2·($\mathrm{CO}$ selectivity) exactly. Feeding fresh $\mathrm{H_2}$ and $\mathrm{CO_2}$ at the methanol stoichiometry of 3.0 therefore injects a surplus with no exit but the purge, and since hydrogen recycles while carbon is consumed, it accumulates: the reactor inlet reaches 94 mol% hydrogen and the $\mathrm{CO_2}$ partial pressure is throttled from 20 bar to 3 bar.

What that costs, and what it does not. In an adiabatic bed the accumulation is disabling: the loop will not close below a 10 % purge at R = 3.0, and trimming the ratio to 2.40 was the only way to reach an industrial purge at all. In the cooled bed it is not disabling. The loop closes to 1 % purge, and carbon yield rises monotonically with R across the entire grid:

| Purge \ R | 2.40 | 2.70 | 2.85 | 2.95 | 3.00 |
|---|---|---|---|---|---|
| 10 % | 74.4 % | 81.6 % | 84.3 % | 85.5 % | 86.0 % |
| 5 % | 77.5 % | 86.3 % | 89.9 % | 91.6 % | 92.0 % |
| 3 % | 78.5 % | 88.0 % | 92.2 % | 94.3 % | 94.8 % |
| 2 % | fails | fails | 93.3 % | 95.8 % | 96.4 % |
| 1 % | fails | 89.3 % | 94.2 % | 97.2 % | 98.0 % |

So the accumulation sets the recycle ratio, not the feasibility. Trimming R buys a compact loop, not a better yield, and the two columns of that trade are laid out in Section 3b. Two earlier claims in this document were wrong and are corrected here: that the bed alone set the purge limit, and that feeding at 3.0 was a trap instead of a design choice with a stated price.

The obvious prescription, re-tested. The natural fix is to drive the reactor inlet to the stoichiometric SN of about 2.1. In the cooled bed at a 3 % purge, which is where this sweep was measured:

| Inlet SN target | Fresh ratio needed | Carbon yield |
|---|---|---|
| 2.10 | 2.75 | 89.5 % (target not reached, floors at SN 3.84) |
| 3.00 | 2.70 | 88.0 % |
| 5.00 | 2.80 | 90.9 % |
| 8.00 | 2.88 | 93.0 % |
| 12.0 | 2.94 | 94.2 % |
| 17.35 | 3.00 | 94.9 % |

In the cooled bed the stoichiometric target is not reachable, because the controller floors at SN 3.84, and yield strictly increases with the target. The general conclusion survives in stronger form. A stoichiometric target belongs on the fresh feed, not on the reactor inlet. At the inlet, SN is an outcome, and for this loop the best available value of that outcome is simply the highest one.

What is implemented. `RecycleLoopConfig::feed_ratio_mode` offers three policies. `FromArgument` is the default and reproduces every historical number bit for bit, asserted in the tests. `FixedRatio` sets hydrogen from a configured ratio. `TargetInletSN` runs a bounded secant on the ratio and reports whether the target was actually reached instead of returning the nearest endpoint as if it had converged.

Resolved. The canonical design point now uses the trimmed ratio and the cooled reactor; see section 3b. What remains genuinely open is the catalyst charge question in section 8, question 1.

### Scope limits

- No dynamics. Steady state only.
- No compressor performance curves. Fixed isentropic and mechanical efficiency.
- The distillation column is a lumped recovery and purity model, not a stage-by-stage calculation.
- Carbon dioxide has no NRTL binary parameters in the ChemSep databank. This is bounded instead of ignored: at 78 bar the flash takes the fugacity route, which does have $\mathrm{CO_2}$ interaction parameters. It would bite at low pressure.

---

## 6. Sources

| Reference | Used for |
|---|---|
| Van-Dal and Bouallou (2013), *J. Cleaner Production* 57, 38-45 | Flowsheet, kinetic constants, catalyst charge, operating conditions, laboratory validation case |
| Mucci et al. (2023) | PEM electrolyser efficiency polynomial, hydrogen compressor, storage, recycle and membrane split fractions |
| Lim et al. (2022) | Stoichiometric number definition, economics, prices |
| Shi et al. (2020) | Tube geometry, distillation recovery and purity |
| Turton et al., 5th ed. | Equipment costing, vessel and column sizing |
| Xu and Froment (1989) | Reforming kinetics |
| Aboosadi et al. (2011) | Tri-reforming parameters and effectiveness factors |
| Fichtl et al. (2015) | Catalyst deactivation |
| Vanden Bussche and Froment (1996) | Methanol synthesis rate form |
| Graaf et al. (1986) | Equilibrium constants |
| ChemSep databank | NRTL binaries, Peng-Robinson interaction parameters |
| DIPPR correlations | Heat capacity, vapour pressure, viscosity |
| DECHEMA / Gmehling and Onken | Methanol-water VLE validation data |

---

## 7. How to build and check it yourself

```
cmake -S. -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Requires a C++17 compiler and CMake 3.16 or later. No external libraries.

Each test binary is self-contained, prints PASS or FAIL per assertion with the computed and expected values side by side, and returns non-zero on failure. To check a single claim, run the relevant binary directly and read the output; the numbers in section 4 are all visible in the test output.

`src/main.cpp` runs the full pipeline and prints a design-point summary, including a model fingerprint: a hash over the compiled constant set, so two datasets can be compared for whether they came from the same physics.

---

## 8. Questions where a second opinion would help most

These are the places where I am least confident, ordered by how much a wrong answer would matter.

1. Van-Dal's own industrial figures do not agree with each other under this model, and I cannot tell which one to trust. The paper states a 1 percent purge, a recycle ratio of 5.0, a per-pass conversion of 33 percent and an overall conversion of about 93 percent. Holding the sourced purge and sweeping only the feed ratio, those three point to R values of roughly 2.86, 2.92 and below 2.85 respectively. They bracket a narrow band but no single operating point reproduces all three. Chapter 24 sets out the three candidate explanations. Is one of them clearly right, and does the disagreement weaken the validation claim more than I have allowed for?

2. Is the feed ratio defensible, given that it is the one unsourced number in the design point? The purge is sourced, the charge is sourced, the bed geometry is sourced. R = 2.95 is not: it is chosen at the knee of the yield-against- compression curve, and the curve is the whole justification. Going to 3.00 buys
0.85 points of yield for 14 MW and drops the ageing floor from 0.45 to 0.80; going to 2.90 saves 1.8 MW and gives up 1.4 points. Is presenting it that way, as a design choice with the trade curve attached instead of as an optimum, sufficient? And is the compact loop at R = 2.40 better presented as a co-equal branch or as a sensitivity case?

2b. The design point sits next to a hole in the feasible set. At a 1 percent purge the loop converges at R = 2.70 and at 2.85 but not at 2.75 or 2.80. R = 2.95 has margin on both sides and in purge from 0 to 2 percent, which is part of why it was chosen over the slightly cheaper R = 2.85. Is that the right weighting of robustness against cost, and should the non-convexity be a stated limitation of the sampling interface instead of only a documented observation?

3. The derived heat-transfer coefficient. U is derived, not sourced, and it sets the cooling duty. The derivation is in `docs/U_Derivation.md`. Is the assumption behind it reasonable?

4. The shift-constant sign correction. I concluded Van-Dal's Eq. (9) is printed with inverted signs and implemented Graaf's original instead. The evidence is in doc 08. This is the single change most likely to be wrong in a way that would bias every result.

5. Is the lumped distillation model acceptable at this level? It reports recovery and purity but has no stage count or reflux internally. The column diameter calculation borrows Van-Dal's reflux ratio and stage count externally.

6. Anything in section 5 that should be a blocker instead of a caveat. My judgement is that flagged placeholders are publishable because they are visible and bounded. A reviewer may disagree, particularly on the tri-reformer bed geometry.

---

## 9. Release state

Everything in the tree now ships together: 48 translation units, 38 test binaries, 1004 assertions, zero warnings under `-Wall -Wextra`, and 25 documents covering every module.

The physical core remains separable and is still checked that way. Its code is 24 translation units, 16 test binaries and 420 assertions, and the earliest layers still compile in isolation from their own files plus earlier ones only.

That separability was always a property of the code instead of of the documents. Documents 07, 12, 16, 17 and 19 belong to the physical core but quote operating-point numbers that only the flowsheet layer can produce, so while the upper layers were held back those sections could not be reproduced from the sources shipped beside them. Releasing the whole tree at once resolves it, and it is the reason the upper documentation was finished instead of the affected sections rewritten in unit-level terms.

What remains outside the scope of this release is unchanged and is listed in Section 5: the flagged placeholders, the derived heat transfer coefficient, and the dispatch rule's status as a heuristic instead of a reproduction of any published optimisation. 