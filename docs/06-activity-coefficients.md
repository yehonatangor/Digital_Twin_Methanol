# Activity Coefficients

The NRTL model for liquid-phase non-ideality, its parameter set, and a statement of what it does not cover.

## Background: why liquids need a second model

An equation of state describes both phases through fugacity coefficients, so in principle no separate liquid model is required. In practice cubic equations struggle with liquids whose non-ideality comes from hydrogen bonding, because the attraction term averages over all interactions and cannot represent a directional bond between specific molecules. Methanol and water are exactly that case.

The alternative is to treat the liquid as a solution and measure its departure from ideality directly. Raoult's law holds for an ideal solution:

$$p_i = x_i P_i^{\text{sat}}$$

which assumes a molecule of $i$ feels the same environment surrounded by $j$ as surrounded by its own kind. The activity coefficient $\gamma_i$ is defined as the correction:

$$p_i = \gamma_i x_i P_i^{\text{sat}}$$

$\gamma_i > 1$ means unlike molecules repel relative to like ones, so component $i$ escapes to the vapour more readily than Raoult predicts, giving a positive deviation. $\gamma_i < 1$ means unlike molecules attract more strongly, as in chloroform and acetone. Values pass through unity at $x_i = 1$ by construction, since a pure component is its own reference.

The limiting value $\gamma_i^\infty$ at infinite dilution is the most demanding test of a model, because that is where a molecule is surrounded entirely by foreign neighbours and the non-ideality is largest. It is also where the verification below concentrates its reference points.

### Local composition theory

Early models such as Margules and van Laar expanded excess Gibbs energy as a polynomial in composition. They fit binaries adequately and extrapolate to multicomponent mixtures poorly, because a polynomial carries no physical picture of what the molecules are doing.

Wilson (1964) introduced the idea that fixed the problem. In a non-ideal mixture, the composition immediately surrounding a given molecule is not the bulk composition. If $i$-$j$ interactions are more favourable than $j$-$j$, then $i$ molecules are over-represented in the neighbourhood of a $j$ molecule. The local mole fractions follow a Boltzmann weighting on interaction energy:

$$\frac{x_{ji}}{x_{ii}} = \frac{x_j \exp\!\left(-g_{ji}/RT\right)}{x_i \exp\!\left(-g_{ii}/RT\right)}$$

where $g_{ji}$ is the energy of a $j$-$i$ interaction. Excess Gibbs energy is then built from local rather than bulk compositions. This carries real physics, so binary parameters fitted on binary data predict ternary and higher mixtures without further fitting, which is what makes the approach useful for a flowsheet.

Wilson's own equation has one defect: it cannot represent liquid-liquid phase splitting. Renon and Prausnitz (1968) generalised it by adding a third parameter $\alpha$, the non-randomness factor, which scales how strongly local composition departs from bulk. Setting $\alpha = 0$ recovers an ideal random mixture. Typical fitted values are 0.2 to 0.47, and 0.3 is a common default. The resulting NRTL model handles both vapour-liquid and liquid-liquid equilibrium, and it is the standard choice for alcohol and water systems.

UNIQUAC (Abrams and Prausnitz 1975) adds molecular size and surface area terms and is the other common choice. NRTL is used here because the parameters available for methanol and water are NRTL parameters.

## Where this applies

Below 10 bar the flash uses a $\gamma$-$\varphi$ formulation, in which liquid non-ideality is carried by an activity coefficient rather than by a fugacity coefficient from the equation of state:

$$K_i = \frac{\gamma_i P_i^{\text{sat}}}{P}$$

The relevant liquid here is condensed crude methanol, a methanol and water mixture with dissolved light gases. Methanol and water are strongly non-ideal, so an ideal-solution assumption would misplace the separation.

## The NRTL equation

Renon and Prausnitz (1968). For component $i$ in a multicomponent liquid:

$$\ln\gamma_i = \frac{\sum_j \tau_{ji} G_{ji} x_j}{\sum_k G_{ki} x_k} + \sum_j \frac{x_j G_{ij}}{\sum_k G_{kj} x_k} \left[\tau_{ij} - \frac{\sum_m x_m \tau_{mj} G_{mj}}{\sum_k G_{kj} x_k}\right]$$

with

$$G_{ij} = \exp(-\alpha_{ij}\tau_{ij}), \qquad \tau_{ij} = a_{ij} + \frac{b_{ij}}{T}$$

$\tau_{ii} = 0$ and $G_{ii} = 1$ by definition, so a pure component has $\gamma = 1$.

The expression looks unwieldy but its parts are readable. $\tau_{ij}$ is the interaction energy difference $(g_{ij} - g_{jj})/RT$, so it measures how much an $i$-$j$ contact differs from a $j$-$j$ contact in units of thermal energy. $G_{ij}$ is the Boltzmann factor built from it, weighted by the non-randomness parameter, and it is the quantity that converts bulk mole fractions into local ones. Every denominator of the form $\sum_k G_{ki}x_k$ is a normalisation over the local neighbourhood of a molecule of type $i$. The temperature dependence enters only through $\tau$, which is why interaction parameters are stored as $a + b/T$ and why a parameter set fitted at one temperature extrapolates imperfectly to another.

## Asymmetry, and why it is the thing to get right

**$\tau_{ij} \ne \tau_{ji}$.** The parameter describes the energy of an $i$-$j$ interaction relative to a $j$-$j$ interaction, and swapping the reference changes the number. The non-randomness factor $\alpha$ is symmetric; the $\tau$ values are not.

This is the failure mode worth guarding against. If the two halves of a binary pair are exchanged, the model does not crash, does not produce a non-physical number, and does not fail a transcription check. It exchanges the two infinite-dilution activity coefficients, so both remain plausible and both are assigned to the wrong component. The result is a systematically wrong separation that looks correct.

## Parameters

Source: the ChemSep binary interaction databank, NRTL table, drawn from DECHEMA at 1 atm.

The databank stores interaction energies in $\mathrm{cal\,mol^{-1}}$, with $\tau_{ij} = A_{ij}/(RT)$. This project stores the pre-divided form in kelvin, $\tau_{ij} = b_{ij}/T$, so the two representations differ by $R = 1.987204\ \mathrm{cal\,mol^{-1}\,K^{-1}}$:

| | ChemSep ($\mathrm{cal\,mol^{-1}}$) | $\div R$ | Stored |
|---|---|---|---|
| $A_{12}$, methanol to water | −189.0469 | −95.13211 | −95.13209 |
| $A_{21}$, water to methanol | +792.8020 | +398.95350 | +398.95345 |
| $\alpha$ | 0.2999 | | 0.2999 |

Agreement to seven significant figures. Anyone comparing the stored values against the raw databank will see −189.05 rather than −95.13 and should apply this factor before concluding there is a discrepancy.

The orientation is confirmed from the databank's own record ordering: methanol is the first component and its outward interaction is the negative one.

## Coverage, and the gap

A full sweep of the ChemSep NRTL table against this component set returns exactly one parameterised pair, methanol and water.

The databank contains **no carbon dioxide entries at all**, so $\mathrm{CO_2}/\mathrm{CH_3OH}$ and $\mathrm{CO_2}/\mathrm{H_2O}$, the two pairs a real crude methanol condensate most needs, are unavailable from this source.

Unparameterised pairs fall back to $\tau = 0$ and $G = 1$, which is an ideal contribution. That fallback is reported:

```cpp
bool has_binary_params(Species i, Species j);
int  unparameterised_pairs(const Stream& liquid);
```

and the flash result carries the count, so a caller can filter or weight on it.

Practical impact is bounded. The $\gamma$-$\varphi$ path is only selected below 10 bar. Both knockout drums and the interstage condenser run at 78 bar and therefore use $\varphi$-$\varphi$, which does have carbon dioxide interaction parameters. The gap affects low-pressure separations only.

Closing it requires DECHEMA Chemistry Data Series Volume I or a primary vapour-liquid equilibrium paper. It is a sourcing decision, not something a databank query can resolve.

## Verification

Transcription checks confirm digits. They cannot confirm that the model reproduces a measured phase equilibrium, so the verification here is a bubble curve against experimental data.

The bubble point condition at fixed pressure is

$$\sum_i x_i\,\gamma_i(T)\,P_i^{\text{sat}}(T) = P$$

solved for $T$, with the vapour composition following as $y_i = x_i\gamma_i P_i^{\text{sat}}/P$. This exercises the activity model and both DIPPR-101 vapour pressure correlations at once.

Against Gmehling and Onken (DECHEMA Chemistry Data Series Volume I) at 101.325 kPa:

| $x_{\mathrm{MeOH}}$ | $y$ computed | $y$ measured | $T$ computed ($^\circ\mathrm{C}$) | $T$ measured ($^\circ\mathrm{C}$) |
|---|---|---|---|---|
| 0.02 | 0.1328 | 0.134 | 96.61 | 96.4 |
| 0.05 | 0.2745 | 0.267 | 92.55 | 92.9 |
| 0.10 | 0.4244 | 0.418 | 87.65 | 87.7 |
| 0.20 | 0.5841 | 0.579 | 81.60 | 81.7 |
| 0.30 | 0.6727 | 0.665 | 77.87 | 78.0 |
| 0.40 | 0.7349 | 0.729 | 75.16 | 75.3 |
| 0.50 | 0.7856 | 0.779 | 72.96 | 73.1 |
| 0.60 | 0.8310 | 0.825 | 71.04 | 71.2 |
| 0.70 | 0.8741 | 0.870 | 69.27 | 69.3 |
| 0.80 | 0.9163 | 0.915 | 67.62 | 67.6 |
| 0.90 | 0.9582 | 0.958 | 66.04 | 66.0 |

Maximum deviation 0.008 in $y$ and 0.35 K in temperature across the full composition range.

Endpoints, which are definitional: water boils at 100.02 $^\circ\mathrm{C}$ computed against 100.00 $^\circ\mathrm{C}$, methanol at 64.53 $^\circ\mathrm{C}$ against a literature 64.7 $^\circ\mathrm{C}$.

### The swap test

The orientation guard is verified by running the same suite against deliberately exchanged parameters. The exchange is detected through four independent routes, so relaxing any one check does not blind the test:

| Check | Correct | Swapped |
|---|---|---|
| $\tau$ sign, methanol to water | negative | positive |
| $y$ at $x = 0.02$ | 0.1328 | 0.1068 |
| $y$ at $x = 0.10$ | 0.4244 | 0.3908 |
| $T$ at $x = 0.10$ ($^\circ\mathrm{C}$) | 87.65 | 89.33 |
| $\gamma^\infty$, methanol in water | 2.40 | 1.68 |

Twenty-one assertions fail on the swapped set. The correct set passes all sixty-nine.

Note that both parameter sets reproduce the pure-component endpoints and both produce plausible-looking curves in isolation. The dilute-methanol region is where they separate, which is why the reference points are weighted there.

`nrtl.hpp` declares the interface including the coverage queries. 

`nrtl.cpp` holds the parameter table with its unit conversion note and the activity coefficient expression.

`tests/test_vle.cpp` performs the bubble curve comparison and the orientation contract.

