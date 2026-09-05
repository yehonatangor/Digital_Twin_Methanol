# Overview

A quasi-steady-state process model of a green methanol plant, built from first principles in C++ with every numerical constant traced to a primary source.

## Background: methanol and why the route matters

Methanol is the simplest alcohol and one of the largest-volume commodity chemicals, at roughly 100 million tonnes a year. Most of it becomes formaldehyde, acetic acid, olefins through the methanol-to-olefins route, or fuel blendstock. Nearly all of it is made from natural gas or coal by steam reforming to synthesis gas followed by catalytic synthesis, which puts fossil carbon into every tonne.

The synthesis step itself has been industrial since 1923, when BASF ran it at 300 bar over a zinc chromite catalyst. The modern process dates from ICI's copper, zinc oxide and alumina catalyst in 1966, which is active enough to run at 50 to 100 bar and 200 to 300 $^\circ\mathrm{C}$. That catalyst is still the industrial standard and it is the one modelled here.

Three features of the chemistry shape every design decision in this project.

1. The reaction is equilibrium limited. 

    Synthesis is exothermic and converts four moles of gas into two, so both enthalpy and entropy favour the reactants as temperature rises. Per-pass conversion is therefore low, typically 20 to 40 percent, and the plant recovers the product and recycles the rest. The recycle loop is what makes the process viable.

2. Heat removal governs the reactor design.

    At around 50 kJ per mole released in a bed that is already equilibrium limited, a temperature rise costs conversion twice over: it moves equilibrium the wrong way and it accelerates catalyst sintering. Industrial reactors are either water-cooled multitubular units or quench converters for this reason.

3. Carbon dioxide behaves differently from carbon monoxide. 
    Conventional plants feed synthesis gas rich in $\mathrm{CO}$, which needs two hydrogens per carbon and makes no water. Feeding $\mathrm{CO_2}$ instead needs three hydrogens and produces a mole of water per mole of methanol. That water dilutes the product, adds a separation duty, and accelerates catalyst deactivation. It is also why the hydrogen demand of a $\mathrm{CO_2}$ route is 50 percent higher per unit of product, which is the dominant cost term when the hydrogen comes from electrolysis.

The two routes modelled here address that hydrogen cost differently. Direct hydrogenation buys it all from the electrolyser.Tri-reforming makes part of the carbon-bearing feed into $\mathrm{CO}$ instead, lowering the electrolytic hydrogen demand at the price of a reformer and a natural gas feed.

## The problem

Methanol made from captured carbon dioxide and electrolytic hydrogen is a proposed route to a liquid fuel with no fossil carbon in it. Whether that route is economic depends on quantities that interact: how much hydrogen the electrolyser makes per unit of electricity, how far the synthesis reaction proceeds before equilibrium stops it, how much of the unconverted gas is worth recycling, how fast the catalyst loses activity, and what the electricity cost was at the hour the hydrogen was made.

Commercial flowsheet simulators answer these questions, and answer them well, but they are closed. A published techno-economic study reports the outcome without the model that produced it, so the sensitivity of that outcome to any one assumption cannot be tested by a reader.

This project builds the model in the open so that the assumptions are visible and the sensitivity is computable.

## What it is

Two production routes are modelled end to end and share the same physical core.

i. Direct hydrogenation.

Captured carbon dioxide reacts with electrolytic hydrogen over a copper, zinc oxide and alumina catalyst:

$$\mathrm{CO_2} + 3\,\mathrm{H_2} \rightleftharpoons \mathrm{CH_3OH} + \mathrm{H_2O}$$

$$\mathrm{CO_2} + \mathrm{H_2} \rightleftharpoons \mathrm{CO} + \mathrm{H_2O}$$

The second reaction, the reverse water gas shift, competes for hydrogen and sets the selectivity.

ii. Tri-reforming.

Natural gas is converted to synthesis gas by the combined action of steam, carbon dioxide and oxygen, then that gas is sent to the same synthesis loop. The oxygen comes from the electrolyser as a byproduct.

$$\mathrm{CH_4} + \mathrm{H_2O} \rightleftharpoons \mathrm{CO} + 3\,\mathrm{H_2}$$

$$\mathrm{CH_4} + \mathrm{CO_2} \rightleftharpoons 2\,\mathrm{CO} + 2\,\mathrm{H_2}$$

$$\mathrm{CH_4} + 2\,\mathrm{O_2} \rightarrow \mathrm{CO_2} + 2\,\mathrm{H_2O}$$

Combustion supplies the heat the two endothermic reforming reactions consume, which is the reason the three are run together.

## The governing rule

Every numerical constant in this codebase is either traceable to a named source, or marked as not traceable.

That constraint shaped the architecture. A value with no source does not get a plausible substitute. It gets a flag that the test suite asserts on, so the gap cannot close by accident:

```cpp
double U_W_m2K   = 980.0;
bool   U_sourced = false; // derived under a stated assumption
```

The consequences run through the whole design. Unsourced viscosity data returns a negative value. Reference values used in tests are drawn from a different source than the correlations they check, so a transcription error cannot pass by comparing the code against itself. Where the code deliberately departs from a cited equation, the departure is recorded at the point of departure with the evidence for it.

## Scope of the model

The reactor is a one-dimensional plug flow model integrated over catalyst mass, with coupled mole balance, energy balance and pressure drop. Phase separation uses a real vapour-liquid flash with a cubic equation of state above 10 bar and an activity model below it. Catalyst deactivation follows a power law fitted to published sintering data. Equipment costs follow the Turton correlations.

Everything is steady state. Plant state evolves across a sequence of steady solutions, which is the same construction most commercial plant twins use, and which is why the model is described as quasi-steady-state.

## Validation

Agreement with published values, computed by the model rather than asserted:

| Quantity | Source value | Model | Deviation |
|---|---|---|---|
| Per-pass $\mathrm{CO_2}$ conversion | 33 % (Van-Dal 2013) | 33.84 % | 2.5 % |
| Two-stage pressure bookkeeping | 1 bar per stage (Mucci 2023) | 75 bar in, 73 bar out | exact |
| Bare module cost, Example 7.14 | \$797,000 (Turton) | \$797,000 | exact |
| Cost escalated to 2016 | \$1,088,100 (Turton) | \$1,088,100 | exact |
| Deactivation constants, six points | Fichtl Table 4 | reproduced | exact |
| Reforming rate constants | Xu and Froment Table 5 | within published confidence interval | |
| Tri-reformer outlet $\mathrm{CH_4}$ | 99.6 % (Aboosadi 2011) | 96.2 % | 3.4 % |
| Formation enthalpy and entropy, nine species | NIST-JANAF and CRC | | exact to 0.01 |
| Methanol and water bubble curve | DECHEMA, eleven points | | $< 0.008$ in $y$ |

The test suite enforces two additional properties not covered by literature comparisons: first, the reactor results must remain invariant under a fourfold refinement of the integration step; second, the carbon, hydrogen, and oxygen balances must close within $10^{-10}$. These checks are necessary because a validation case can spuriously converge while the active simulation does not, and a total mass balance can remain satisfied even if individual species are artificially generated or consumed.

## Limitations

- Distillation and the membrane are lumped recovery and purity models instead of stage-by-stage solutions; neither source paper publishes the tray-level equilibrium data a rigorous column requires.

- The activity model covers one binary pair, methanol and water. The ChemSep databank contains no carbon dioxide entries at all, so those interactions fall back to ideal solution. The flash reports how many pairs it treated as ideal.

- Reactor tube geometry is a declared substitution. Van-Dal publishes the catalyst but not the tubes, so Shi's geometry is adopted
and labelled as the design basis.

- The dispatch schedule and the reactor are evaluated alongside each other instead of being coupled per timestep.

- No heat transfer coefficient is supplied anywhere without derivation; no source paper publishes one.

## Conventions

| Quantity | Convention |
|---|---|
| Composition | molar flow, $\mathrm{mol\,s^{-1}}$, as the internal source of truth |
| Pressure | $\mathrm{Pa}$ internally, bar at kinetic interfaces |
| Temperature | $\mathrm{K}$ internally, $^\circ\mathrm{C}$ in presets |
| Reactor basis | per tube; scaling to plant basis happens at the flowsheet boundary |
| Catalyst coordinate | mass $W$, $\mathrm{kg}$, not axial length |
| Cost basis | 2001 USD, CEPCI-escalated to a stated year |

## Build

```
cmake -S . -B build
cmake --build build -j8
ctest --test-dir build --output-on-failure
```

C++17, CMake 3.16 or later, no external dependencies.
