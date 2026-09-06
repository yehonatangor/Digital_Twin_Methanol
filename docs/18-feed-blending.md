# Feed Blending

Combining a reformed gas stream with electrolytic hydrogen, and the single number that says whether the blend can react to completion.

## Background: syngas quality and the module ratio

Synthesis gas is characterised not by its absolute composition but by a ratio that says whether the hydrogen supply matches what the carbon requires. In the methanol industry this is the module, or stoichiometric number, and hitting it is the single most important specification on a reformer outlet.

The reason a single ratio suffices is that only three components matter. Two reactions consume the carbon:

$$\mathrm{CO} + 2\,\mathrm{H_2} \rightarrow \mathrm{CH_3OH}$$

$$\mathrm{CO_2} + 3\,\mathrm{H_2} \rightarrow \mathrm{CH_3OH} + \mathrm{H_2O}$$

and the water gas shift interconverts $\mathrm{CO}$ and $\mathrm{CO_2}$ freely inside the loop, so the split between them adjusts itself. What cannot adjust is the total hydrogen against the total carbon, which is what the module measures.

A conventional steam methane reformer produces a module near 2.9, which is hydrogen-rich. Plants correct it by importing carbon dioxide, exporting hydrogen as fuel, or adding an autothermal or partial oxidation step. Dry and tri-reforming produce modules below 2, which is hydrogen-lean, and the correction is to add hydrogen. In this project that hydrogen comes from the electrolyser, which is what couples the two halves of the plant.

Industrial practice targets slightly above the stoichiometric value, commonly
2.0 to 2.1. A small hydrogen excess keeps the catalyst reduced, suppresses coke formation, and improves selectivity by keeping the surface hydrogen-rich. The penalty is a larger recycle and a larger purge.

### Why the temperature of a mixed stream is not an average

Mixing two streams conserves enthalpy, not temperature. Writing the balance:

$$\sum_s \sum_i F_{i,s} H_i(T_s) = \sum_i F_i^{\text{mix}} H_i(T_{\text{mix}})$$

the outlet temperature is whatever satisfies this equality, and it equals a mole-weighted average of the inlet temperatures only when all species share the same constant heat capacity. They do not. Hydrogen at 2.016 g/mol and carbon dioxide at 44.01 g/mol have very different molar heat capacities, and both vary with temperature.

The error from a mole-weighted average grows with the temperature difference between the streams, which is exactly the case here: reformed gas near 1100 K meeting electrolytic hydrogen near 340 K. The module therefore solves the enthalpy balance rather than averaging.

## The problem

Two carbon-bearing routes feed the same synthesis loop. Tri-reforming produces a syngas that is carbon-rich and hydrogen-poor relative to what methanol synthesis needs. Electrolysis produces pure hydrogen. Blending them is the point where the hybrid plant becomes one plant rather than two.

The blend is not free. Too little hydrogen and carbon leaves unconverted. Too much and hydrogen is recycled endlessly or purged, which wastes the most expensive input in the flowsheet.

## Mixing

Component molar flows add:

$$F_i^{\text{mix}} = \sum_{s} F_{i,s}$$

Temperature follows from an adiabatic enthalpy balance instead of a mass average, solved with the same Newton iteration used for single-stream heating:

$$\sum_s \sum_i F_{i,s} H_i(T_s) = \sum_i F_i^{\text{mix}} H_i(T_{\text{mix}})$$

A mole-weighted average temperature would be wrong whenever the streams have different heat capacities, which they always do here: hydrogen at 2.016 $\mathrm{g\,mol^{-1}}$ and carbon dioxide at 44.01 $\mathrm{g\,mol^{-1}}$ carry very different enthalpy per mole. Reformed gas at 1100 K meeting hydrogen at 340 K is exactly the case where the difference shows.

Pressure takes the minimum of the inlets, since a stream cannot be mixed into a higher-pressure header without being compressed first. The result flags when the inlet pressures differ by more than a stated tolerance, because that difference is a compression duty someone has to pay.

## Stoichiometric number

The governing quantity for methanol synthesis feed quality:

$$\mathrm{SN} = \frac{F_{\mathrm{H_2}} - F_{\mathrm{CO_2}}}{F_{\mathrm{CO}} + F_{\mathrm{CO_2}}}$$

The target is $\mathrm{SN} = 2.0$.

### Where the expression comes from

Source. Lim, J. et al. (2022), "Process design and economic analysis", their Eq. (4), which in turn cites Andika et al. (2018). The definition and the target value of 2.0 both come from that equation; `BlendConfig::target_SN` carries it as a caller-overridable default instead of a hard-coded constant, and `front_end/feed_blend.hpp` records the same citation at the point of use.

Two synthesis routes consume hydrogen differently:

$$\mathrm{CO} + 2\,\mathrm{H_2} \rightarrow \mathrm{CH_3OH}$$

$$\mathrm{CO_2} + 3\,\mathrm{H_2} \rightarrow \mathrm{CH_3OH} + \mathrm{H_2O}$$

Carbon monoxide needs two hydrogen per carbon. Carbon dioxide needs three, because one hydrogen pair is spent making the water byproduct.

Subtracting $F_{\mathrm{CO_2}}$ in the numerator charges that extra hydrogen against the supply before the ratio is taken. The denominator counts total carbon available to become methanol. So $\mathrm{SN} = 2$ means the hydrogen supply exactly matches what the carbon mix requires, whatever the split between the two oxides.

The number is diagnostic instead of a control input. Nothing in the reactor reads it. It is computed and reported so a flowsheet result can be read for feed quality without re-deriving it.

| Value | Meaning |
|---|---|
| $< 2$ | hydrogen-lean; carbon cannot fully convert |
| $= 2$ | stoichiometric |
| $> 2$ | hydrogen-rich; excess recycles or purges |

Industrial practice runs slightly above 2, commonly 2.0 to 2.1, because a small hydrogen excess suppresses coke formation and improves selectivity. The model does not enforce that; it reports the value. The 2.0 to 2.1 band is general industry practice instead of a figure taken from any paper in this project's library, and is stated here as context, not as a sourced constant.

### Degenerate case

With no carbon oxides at all the denominator vanishes. The code returns a sentinel instead of a division result, because a pure hydrogen stream has no stoichiometric number to speak of and returning infinity or a large finite value would propagate as a plausible-looking figure.

## Sizing the hydrogen make-up

Inverting the definition gives the hydrogen flow that hits a target:

$$F_{\mathrm{H_2}}^{\text{required}} = \mathrm{SN}_{\text{target}}\left(F_{\mathrm{CO}} + F_{\mathrm{CO_2}}\right) + F_{\mathrm{CO_2}}$$

and the make-up needed is the shortfall against what the reformed gas already carries:

$$F_{\mathrm{H_2}}^{\text{make-up}} = \max\left(0,\; F_{\mathrm{H_2}}^{\text{required}} - F_{\mathrm{H_2}}^{\text{reformed}}\right)$$

This is what couples the electrolyser size to the reformer size. Given a reformed gas composition and a target stoichiometric number, the hydrogen demand is determined, and from that the electrolyser power follows through the efficiency polynomial.

The clamp at zero matters: a reformed gas can be hydrogen-rich, in which case no make-up is needed and a negative flow would be meaningless.

## Worked example

Take a reformed gas carrying, in $\mathrm{mol\,s^{-1}}$, $F_{\mathrm{CO}} = 30$, $F_{\mathrm{CO_2}} = 10$, $F_{\mathrm{H_2}} = 60$.

As delivered:

$$\mathrm{SN} = \frac{60 - 10}{30 + 10} = \frac{50}{40} = 1.25$$

Hydrogen-lean, as expected from a reformer. For $\mathrm{SN} = 2$:

$$F_{\mathrm{H_2}}^{\text{required}} = 2(40) + 10 = 90$$

$$F_{\mathrm{H_2}}^{\text{make-up}} = 90 - 60 = 30\ \mathrm{mol\,s^{-1}}$$

Thirty moles per second is 0.0605 $\mathrm{kg\,s^{-1}}$. At an electrolyser efficiency near
0.75 and a lower heating value of 119.96 $\mathrm{MJ\,kg^{-1}}$, that is roughly 9.7 MW of stack power, before auxiliaries. The reformer feed composition sets the electrolyser rating.

Verifying the blend: hydrogen becomes 90, carbon oxides unchanged, so $\mathrm{SN} = (90-10)/40 = 2.0$.

## Verification

| Check | Result |
|---|---|
| Component conservation | mixed flow equals the sum of inlets, exactly |
| Mixed temperature | enthalpy balance closes to $10^{-9}$ |
| Equal-temperature inlets | mixed temperature returns that value |
| Pressure | minimum of inlets; mismatch flagged |
| Stoichiometric number | reproduces the worked example, 1.25 then 2.0 |
| No carbon oxides | sentinel returned, no division |
| Make-up on a hydrogen-rich feed | clamped to zero |
| Round trip | applying the computed make-up lands on the target |

In the code. `front_end/feed_blend.hpp` and `.cpp` hold the mixer, the stoichiometric number and its inverse, with the two synthesis reactions written out above the definition so the origin of the expression is readable where it is used.