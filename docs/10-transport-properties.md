# Transport Properties

Gas viscosity for the pressure drop calculation, from pure-component correlations and a mixing rule.

## Why this exists separately

None of the process papers this model is built from publish a gas viscosity or name a viscosity method. Van-Dal states only that the built-in Ergun block of a commercial simulator was used, which leaves the transport property implicit.

Viscosity is nonetheless required, because the Ergun equation's viscous term depends on it directly. This module is therefore external to the process literature and is sourced independently.

## Background: gas viscosity and why it rises with temperature

Viscosity measures resistance to shear. In a liquid it arises from intermolecular attraction, so it falls sharply as temperature rises and the molecules escape one another. In a gas the mechanism is entirely different, and so is the trend.

Chapman-Enskog theory treats gas viscosity as momentum transport by molecular diffusion. A molecule in a fast-moving layer wanders into a slow layer and carries its momentum with it, which resists the shear. The rate of that transfer depends on molecular speed, and speed goes as $\sqrt{T}$, so gas viscosity increases with temperature, the opposite of liquid behaviour. Air at 500 K is about 1.5 times as viscous as air at 300 K.

The kinetic theory result for a dilute gas is

$$\mu = \frac{5}{16}\frac{\sqrt{\pi m k_B T}}{\pi \sigma^2 \Omega^{*}(T^{*})}$$

where $\sigma$ is a molecular diameter and $\Omega^{*}$ a collision integral that corrects for the fact that molecules are not hard spheres. Two consequences matter here. Viscosity is independent of pressure in the dilute limit, because increasing density raises the number of momentum carriers and shortens the mean free path in exactly compensating proportions. And light molecules are less viscous than heavy ones at the same temperature, since $\mu \propto \sqrt{m}$, which is why hydrogen at $8.9\ \mu\mathrm{Pa\cdot s}$ is the least viscous species in this mixture.

The pressure independence holds until reduced density becomes significant, above roughly $P_r > 0.5$ or near the critical point, where a Lucas or Chung high-pressure correction is needed. The correlations here are dilute-gas correlations with no such correction, which is stated as a limitation later in this document.

The DIPPR-102 form used below is an empirical fit rather than the kinetic theory expression, but its structure mirrors it: a $T^{B}$ numerator with $B$ near 0.5 to 0.7 reproducing the theoretical square root, and a denominator absorbing the temperature dependence of the collision integral.

## Pure component viscosity

The DIPPR-102 form, with $\mu$ in Pa s and $T$ in kelvin:

$$\mu(T) = \frac{A\,T^{B}}{1 + C/T + D/T^{2}}$$

| Species | $A$ | $B$ | $C$ | $D$ | Valid range (K) |
|---|---|---|---|---|---|
| $\mathrm{CO_2}$ | $2.1480\times10^{-6}$ | 0.46000 | 290.00 | 0 | 194.67 to 1500 |
| $\mathrm{H_2}$ | $1.7970\times10^{-7}$ | 0.68500 | −0.59 | 140 | 13.95 to 3000 |
| $\mathrm{CO}$ | $1.1127\times10^{-6}$ | 0.53380 | 94.70 | 0 | 68.15 to 1250 |
| $\mathrm{H_2O}$ | $1.7096\times10^{-8}$ | 1.11460 | 0 | 0 | 273.16 to 1073.15 |
| $\mathrm{CH_3OH}$ | $3.0663\times10^{-7}$ | 0.69655 | 205.00 | 0 | 240 to 1000 |
| $\mathrm{CH_4}$ | $5.2546\times10^{-7}$ | 0.59006 | 105.67 | 0 | 90.69 to 1000 |
| $\mathrm{N_2}$ | $6.5592\times10^{-7}$ | 0.60810 | 54.714 | 0 | 63.15 to 1970 |
| $\mathrm{Ar}$ | $9.2121\times10^{-7}$ | 0.60529 | 83.240 | 0 | 83.78 to 3273.1 |
| $\mathrm{O_2}$ | $1.1010\times10^{-6}$ | 0.56340 | 96.30 | 0 | 54.35 to 1500 |

### Provenance

The proximate source is the `chemicals` Python package (Bell et al., MIT licence), table `mu_data_Perrys_8E_2_312`. That table is a digitisation of Perry's Chemical Engineers' Handbook, 8th edition, Table 2-312, which tabulates this same DIPPR-102 form as $\{C_1, C_2, C_3, C_4\}$ with the stated validity range.

The distinction between proximate and underlying source is deliberate. Nobody on this project has opened Perry's. Citing it as though it had been read directly would be a secondary citation presented as a primary one.

What makes the chain sound is not the handbook's authority but the independent check below. Every row was validated against a third source before being used, so the digitisation is not taken on trust.

### Verification

Each correlation evaluated at a temperature where an independent CRC or NIST reference viscosity is available.

| Species | $T$ (K) | Computed | Reference | Deviation |
|---|---|---|---|---|
| $\mathrm{CO_2}$ | 300 | $1.506\times10^{-5}$ | $1.500\times10^{-5}$ | +0.39 % |
| $\mathrm{H_2}$ | 300 | $8.944\times10^{-6}$ | $8.950\times10^{-6}$ | −0.06 % |
| $\mathrm{CO}$ | 300 | $1.776\times10^{-5}$ | $1.780\times10^{-5}$ | −0.21 % |
| $\mathrm{H_2O}$ | 373.15 | $1.258\times10^{-5}$ | $1.230\times10^{-5}$ | +2.24 % |
| $\mathrm{CH_3OH}$ | 400 | $1.316\times10^{-5}$ | $1.310\times10^{-5}$ | +0.49 % |
| $\mathrm{CH_4}$ | 300 | $1.125\times10^{-5}$ | $1.120\times10^{-5}$ | +0.44 % |
| $\mathrm{N_2}$ | 300 | $1.780\times10^{-5}$ | $1.790\times10^{-5}$ | −0.56 % |
| $\mathrm{Ar}$ | 300 | $2.277\times10^{-5}$ | $2.270\times10^{-5}$ | +0.31 % |
| $\mathrm{O_2}$ | 300 | $2.073\times10^{-5}$ | $2.070\times10^{-5}$ | +0.12 % |

Water is the loosest at 2.24 %, because Perry's row for water is a bare two-parameter power law with $C = D = 0$. This is still well inside the accuracy any Ergun estimate requires.

## Mixture viscosity

A mixture viscosity is not a mole-fraction average, and the reason is again momentum transport. A heavy molecule moving through a light gas carries far more momentum per collision than its mole fraction suggests, so it obstructs shear disproportionately. Mixtures of light and heavy gases can even show a maximum in viscosity at intermediate composition, exceeding both pure components.

Wilke derived an interaction-weighted average from kinetic theory, with the weighting factor $\phi_{ij}$ built from the viscosity and molar mass ratios of each pair. It reproduces experimental mixture viscosities to a few percent and requires no data beyond the pure-component values.

Wilke (1950), as given in Poling et al., Equation 9-5.13:

$$\mu_{\text{mix}} = \sum_i \frac{y_i \mu_i}{\sum_j y_j \phi_{ij}}$$

$$\phi_{ij} = \frac{\left[1 + \sqrt{\mu_i/\mu_j}\,(M_j/M_i)^{1/4}\right]^{2}}{\sqrt{8\left(1 + M_i/M_j\right)}}$$

For this mixture the difference from a linear average is large. At the Van-Dal feed composition, which is 82 mol% hydrogen, the mixture viscosity is roughly twice that of pure hydrogen. A mole-fraction average would understate the viscous pressure drop term substantially.

Note that $\phi_{ii} = 1$ by construction, so a single-component mixture reduces exactly to that component's pure viscosity.

### Verification

Against an independent implementation of the same mixing rule (`chemicals.viscosity.Wilke`) supplied with the same pure viscosities and molar masses.

| Case | Computed | Independent |
|---|---|---|
| Van-Dal feed at 493.15 K | $2.313266\times10^{-5}$ | $2.313266\times10^{-5}$ |
| Reforming feed at 1100 K | $4.166217\times10^{-5}$ | $4.166217\times10^{-5}$ |

Agreement to machine precision. The Van-Dal case is $\mathrm{CO_2}$ 3, $\mathrm{H_2}$ 82, $\mathrm{CO}$ 4 and $\mathrm{Ar}$ 11 mol%.

## Failure behaviour

An unsourced species returns $-1$ rather than a plausible number, and the mixture rule propagates that sentinel if any species present in the stream lacks data.

This matters more than it appears. A viscosity that is merely wrong produces a pressure drop that is merely wrong, and nothing downstream detects it, because there is no conservation law on pressure drop the way there is on mass. A negative sentinel forces the caller to handle the missing data.

The table is currently complete, so the sentinel path is exercised only by the tests. The mechanism is retained because a tenth species added later would otherwise default to something.

## Temperature range

Every row carries a validity range, and those ranges are enforced by the validity module instead of being stored and ignored.

Two are exceeded by the tri-reforming pathway, which runs above 1100 K: water is fitted to 1073.15 K and methane to 1000 K. This is flagged as an extrapolation on any design point that reaches those temperatures.

In the code. `transport.hpp` declares the coefficient structure, the sentinel contract and the configuration. `transport.cpp` holds the table with its per-row citation and validation deviations, the Wilke rule and the source validation routine.