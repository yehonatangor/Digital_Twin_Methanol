A modular C++17 digital twin of a green methanol plant, built from first principles and grounded in primary literature.

Two production routes are modelled end to end:

1. Direct $\mathrm{CO_2}$ hydrogenation: electrolytic $\mathrm{H_2}$ + captured $\mathrm{CO_2} \rightarrow $ methanol
2. Tri-reforming (TRM): natural gas + $\mathrm{CO_2}$ + steam + $\mathrm{O_2} \rightarrow $ syngas $ \rightarrow $ methanol

Both run through the same thermodynamics, kinetics, reactor, separation, degradation and costing stack.

## The one rule this project runs on

Every number carries a citation, or it is labeled as not having one.

That constraint drove most of the design. Where a required value does not exist in the literature, the code says so, with `PLACEHOLDER, SOURCE NEEDED` or a `*_sourced = false` flag that the test suite asserts on.

Some consequences:

- A component with no sourced viscosity coefficients returns a `-1.0` sentinel. The nine species actually used are sourced from Perry's 8e Table 2-312.

- `tests/reference_data.hpp` holds every reference value in one place with its citation, and labels each one. `INDEPENDENT` means the reference came from a source that did not supply the correlation under test, so agreement is evidence: the DECHEMA bubble curve against NRTL, three papers' reaction enthalpies against the formation table, normal boiling points against DIPPR-101. `TRANSCRIPTION` means the code's table and the reference share a source, so agreement shows only that the digits were copied right. The distinction is stated as validation.

- Where the code knowingly disagrees with a cited paper, and Van-Dal's Eq. (9) has a sign typo, the disagreement is documented at the point of disagreement with the evidence. See `src/lhhw.cpp::Keq2`.

---

## Primary sources

| Ref | Role |
|---|---|
| Van-Dal & Bouallou (2013), *J. Cleaner Prod.* 57, 38–45 | $\mathrm{CO_2}$-hydrogenation plant; LHHW kinetic parameters (Table 3) |
| Mucci et al. (2023), *J. Energy Storage* 72, 108614 | Two-stage cooled reactor, electrolyzer, $\mathrm{H_2}$ storage, plant economics |
| Lim et al. (2022) | Tri-reforming front end; electrolyzer technology comparison |
| Shi et al. (2020), *J. CO₂ Util.* 38, 241–251 | Reactor geometry, purification targets, gas-basis reaction enthalpies |
| Xu & Froment (1989), *AIChE J.* 35(1), 88–96 | SRM / WGS / overall reforming intrinsic kinetics |
| Arab Aboosadi et al. (2011), *Appl. Energy* 88, 2691–2701 | Tri-reformer PFR model, combustion kinetics, validation case |
| Turton et al., *Analysis, Synthesis and Design of Chemical Processes*, 5e | CAPCOST equipment costing, CEPCI escalation |
| Fichtl et al. | $\mathrm{Cu/ZnO/Al_2O_3}$ catalyst deactivation kinetics |
| Perry's Handbook 8e, Table 2-312 | DIPPR-102 vapour viscosity |
| ChemSep databank (LGPL) | Peng-Robinson $k_{ij}$, NRTL binaries |

---

## Build and run

```bash
cmake -S . -B build
cmake --build build -j8
ctest --test-dir build --output-on-failure
./build/methanol_twin # end-to-end demo report
```

Requires a C++17 compiler and CMake ≥ 3.16. No external dependencies.

---

## Layout

```
include/methanol_twin/                          headers only
  units.hpp species.hpp stream.hpp thermo.hpp   core types and properties
  eos.hpp nrtl.hpp flash.hpp                    Peng-Robinson, NRTL, VLE flash
  lhhw.hpp trm_kinetics.hpp                     Methanol + reforming kinetics
  reactor/ transport,                           Ergun, energy balance, RK4 PFR, two-stage
  front_end/                                    Electrolyzer, TRM PFR, compressors, HX, knockout,
                                                Recycle, membrane, distillation, feed blending
  degradation/                                  Catalyst activity decay
  integration/                                  aged reactor
  economics/                                    Turton CAPCOST, vessel/column sizing, plant economics
  dispatch/                                     H2 storage, price-threshold dispatch
  flowsheet/                                    13 composable plant configurations
  sampling/                                     validity ranges, feasibility labels, model fingerprint
src/                                            48 library translation units, mirroring the header tree
  *.cpp                                         foundation: species, thermo, eos, nrtl, flash, kinetics
  reactor/ front_end/ degradation/ integration/
  economics/ dispatch/ flowsheet/ sampling/
  main.cpp                                      end-to-end demo
tests/                                          38 self-contained binaries, 1004 assertions
docs/                                           25-chapter derivation of the entire mathematical apparatus
```

Molar flows are the internal source of truth; mass quantities are derived. Reactor flows and catalyst mass are per tube. Scaling to plant basis happens at the flowsheet boundary.

---

## Validation

Reproductions from the test suite:

| Check | Result |
|---|---|
| Van-Dal per-pass $\mathrm{CO_2}$ conversion (33 %) | 33.84% |
| Mucci two-stage pressure bookkeeping, 1 bar per stage | 75 bar in, 73 bar out |
| Turton Example 7.14, 7 items end to end (\$797,000) | \$797,116 |
| Turton CEPCI-escalated 2016 cost (\$1,088,100) | \$1,088,254 |
| Turton Examples 23.2 / 23.3 vessel diameter | 4.218 m / 0.990 m, exact |
| Turton Example 21.4 column flooding and area | 3.727 $\mathrm{m^{2}}$ against 3.73 |
| Xu & Froment Table 5 rate/adsorption constants | 0.2 to 1.7% |
| Aboosadi Table 8 TRM outlet composition | within 3.4% on $\mathrm{CH_4}$ conversion |
| Fichtl Table 4 deactivation constants (6 points) | exact |
| Formation enthalpy and entropy, 9 species | exact to 0.01 |
| Wilke mixture viscosity vs independent implementation | machine precision |

Beyond agreement with sources, the suite enforces two properties that
literature comparison alone does not catch:

- Grid independence. The reactor result must not change when the integration step is refined 4×. A validation case can be converged while the case you actually run is not; both are checked.

- Atom conservation. C, H and O balances must close to 1e-10. This is strictly stronger than a total-mass check, which can stay green while a single species is silently fabricated.

---

## Known limitations

- Separations are lumped. Distillation and the membrane are recovery/purity split models using published performance figures, not stage-by-stage MESH solutions. Neither source paper publishes the tray-level VLE that a rigorous column would need.

- NRTL covers one binary pair. ChemSep has no $\mathrm{CO_2}$ binaries at all; unparameterised pairs fall back to ideal solution and are counted and reported (`flash::FlashResult::ideal_binary_pairs`)  Bounded impact: the $\gamma$-$\phi$ path is only selected below 10 bar.

- Reactor tube geometry is a declared substitution. Van-Dal publishes the catalyst but not the tubes; Shi's geometry is adopted and labelled as this project's design basis, not as something either paper asserts.

- Dispatch and the reactor are not coupled per timestep. The reactor chain is a steady-state design point evaluated alongside the dispatch schedule.

- The purge fraction is a citation, not a model result. Carbon monoxide leaves dissolved in the crude liquid, so the loop converges even at zero purge. The default takes Van-Dal's stated 1 %. A real plant needs a purge for inert ingress and catalyst poisons that are outside this model's scope.

- The feasible operating region is not convex. At a 1 % purge the recycle loop converges at $\mathrm{H_2{:}CO_2}$ of 2.70 and 2.85 but not at 2.75 or 2.80, so no interval describes it. `validity::check_recycle_loop_before_solving` answers from the 35 measured nodes before a point is solved, and returns `Unmeasured` off-grid instead of interpolating across the gap.

- No heat-transfer coefficient is supplied anywhere. `U` and `UA` have no defaults because no source paper publishes one. Callers must provide them.

- Dispatch is a price-threshold heuristic, not a reproduction of any published scheduling optimisation, and its outputs are not compared against one.

---

## Status

C++17 · CMake + CTest · 48 translation units, 38 test binaries, 1004 assertions, clean under `-Wall -Wextra`. All 25 chapters written; code and documentation ship together.

---

## License

MIT, see `LICENSE`. The code, the build files and the documentation written for this project are covered by it.

`NOTICE.md` records what is not: the correlation constants are values published by other people and cited at the point of use, the Peng-Robinson and NRTL binary parameters come from the LGPL ChemSep databank, and the Turton CAPCOST tables belong to their publisher. None of that is relicensed here.
 Are there any LLM trade marks etc.