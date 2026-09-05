# Species Properties

Nine components, their critical constants, ideal-gas heat capacities and vapour pressures.

## The component set

| Species | Role |
|---|---|
| $\mathrm{CO_2}$ | reactant, and a reforming co-reactant |
| $\mathrm{H_2}$ | reactant |
| $\mathrm{CO}$ | reverse water gas shift product, reforming product |
| $\mathrm{H_2O}$ | byproduct, reforming reactant |
| $\mathrm{CH_3OH}$ | product |
| $\mathrm{CH_4}$ | reforming feed |
| $\mathrm{N_2}$ | inert |
| $\mathrm{Ar}$ | inert, present in Van-Dal's feed at 11 mol% |
| $\mathrm{O_2}$ | partial oxidation reactant |

Argon is carried on its own and is not lumped in with nitrogen because Van-Dal's laboratory feed specifies argon as the inert, and its molar mass differs from nitrogen by 43 %, which matters for a mass flux and therefore for pressure drop.

## Background: why these particular constants

A process model needs three families of pure-component data, and each family
answers a different question.

1. Critical constants answer how a substance departs from ideal gas behaviour.

    The critical point is where the liquid and vapour phases become indistinguishable: their densities converge and the enthalpy of vaporisation falls to zero. Above $T_c$ no pressure will liquefy the substance. The practical value of $T_c$ and $P_c$ is that they set the scale for the principle of corresponding states, which holds that fluids behave similarly when compared at the same reduced conditions $T_r = T/T_c$ and $P_r = P/P_c$. Every cubic equation of state is built on this idea. The parameters $a$ and $b$ are therefore computed from $T_c$, $P_c$ and $\omega$.

    Reduced conditions also tell an engineer at a glance what regime a unit operates in. Hydrogen at 500 K has $T_r = 15$, far supercritical, so it will not condense under any conditions in this process. Methanol at the same temperature has $T_r = 0.98$, just below critical. It condenses in the knockout drum, and its property data has to be accurate for that reason.

2. Heat capacity answers how much energy a temperature change costs. 

    It appears in every energy balance in the project, and its temperature dependence cannot be neglected across the ranges here: methane's ideal-gas $C_p$ more than doubles between 298 K and 1300 K.

3. Vapour pressure answers when a substance condenses. 

    It sets the equilibrium ratios in the low-pressure flash and the latent heat in the condensing cooler.

### Ideal gas heat capacity and where its shape comes from

The heat capacities tabulated below are **ideal gas** values, meaning the
contribution from molecular motion alone with no intermolecular forces. Pressure
corrections are handled separately by the equation of state through a departure
function, which keeps the two effects separable.

Classical equipartition assigns $\tfrac{1}{2}R$ per active degree of freedom. A monatomic gas has three translational modes only, giving $C_p = \tfrac{5}{2}R = 20.79\ \mathrm{J\,mol^{-1}K^{-1}}$, and argon in the table below is exactly that value at every temperature. A diatomic molecule adds two rotational modes, giving $\tfrac{7}{2}R = 29.10$. Nitrogen, oxygen and carbon monoxide all sit near 29 at room temperature for that reason.

The temperature dependence comes from vibration. Vibrational modes are quantised with spacings comparable to $k_BT$ at process temperatures, so they activate gradually as temperature rises rather than contributing fully at all temperatures. Statistical mechanics gives each mode an Einstein contribution proportional to

$$\left[\frac{\theta/T}{\sinh(\theta/T)}\right]^2$$

where $\theta$ is the characteristic vibrational temperature. This is precisely the functional form of the DIPPR-107 correlation used below, which is why that correlation extrapolates far better than a polynomial fit: its shape is the physics rather than a curve drawn through data points.

## Critical constants

| Species | $M$ ($\mathrm{g\,mol^{-1}}$) | $T_c$ (K) | $P_c$ (MPa) | $\omega$ |
|---|---|---|---|---|
| $\mathrm{CO_2}$ | 44.010 | 304.21 | 7.383 | 0.224 |
| $\mathrm{H_2}$ | 2.016 | 33.19 | 1.313 | −0.216 |
| $\mathrm{CO}$ | 28.010 | 132.92 | 3.499 | 0.048 |
| $\mathrm{H_2O}$ | 18.015 | 647.10 | 22.064 | 0.345 |
| $\mathrm{CH_3OH}$ | 32.042 | 512.50 | 8.084 | 0.566 |
| $\mathrm{CH_4}$ | 16.043 | 190.56 | 4.599 | 0.012 |
| $\mathrm{N_2}$ | 28.013 | 126.20 | 3.400 | 0.038 |
| $\mathrm{Ar}$ | 39.948 | 150.86 | 4.898 | −0.004 |
| $\mathrm{O_2}$ | 31.999 | 154.58 | 5.043 | 0.022 |

Verified against the `chemicals` databank, an independent compilation: molar mass agrees to 0.01%, critical temperature to 0.17 %, critical pressure to 1.63%, acentric factor to $\pm 0.004$ absolute.

### The methanol critical pressure

Published values for methanol scatter more than for the other eight species, and the choice propagates directly into the equation of state.

| Source | $P_c$ (MPa) | $T_c$ (K) |
|---|---|---|
| IUPAC | 8.0840 | 512.50 |
| Pina-Martines | 8.0840 | 512.50 |
| Yaws | 8.0970 | 512.64 |
| PSRK | 8.0959 | 512.60 |
| NIST WebBook | 8.1000 | 513.00 |
| CRC | 8.0100 | 512.70 |
| HEOS | 8.2158 | 513.38 |

The IUPAC value is used. It is the authoritative critical-property evaluation and it agrees with the independent Pina-Martines compilation to five figures. The 8.216 MPa outlier is the critical point of a fitted Helmholtz equation of state, which is a property of that correlation rather than a measurement recommendation, and is therefore not the correct basis for a cubic equation of state.

The Peng-Robinson parameters scale as $a \propto 1/P_c$ and $b \propto 1/P_c$, so the 1.6 % spread between IUPAC and HEOS moves methanol's parameters by the same 1.6 %. Measured effect on the knockout drum at $35\ ^\circ\mathrm{C}$ and 78 bar is under one percentage point of methanol recovery, because methanol is already 97 % recovered at that condition.

## Ideal-gas heat capacity

Eight species use the DIPPR-107 form, with $C_p$ in $\mathrm{J\,kmol^{-1}\,K^{-1}}$ and $T$ in kelvin:

$$C_p = A + B\left[\frac{C/T}{\sinh(C/T)}\right]^2 + D\left[\frac{E/T}{\cosh(E/T)}\right]^2$$

| Species | $A$ | $B$ | $C$ | $D$ | $E$ |
|---|---|---|---|---|---|
| $\mathrm{CO_2}$ | 29370 | 34540 | 1428.0 | 26400 | 588.0 |
| $\mathrm{H_2}$ | 27617 | 9560 | 2466.0 | 3760 | 567.6 |
| $\mathrm{CO}$ | 29108 | 8773 | 3085.1 | 8455.3 | 1538.2 |
| $\mathrm{H_2O}$ | 33363 | 26790 | 2610.5 | 8896 | 1169.0 |
| $\mathrm{CH_4}$ | 33298 | 79933 | 2086.9 | 41602 | 991.96 |
| $\mathrm{N_2}$ | 29105 | 8614.9 | 1701.6 | 103.47 | 909.79 |
| $\mathrm{O_2}$ | 29103 | 10040 | 2526.5 | 9356 | 1153.8 |

Argon is monatomic, so its ideal-gas heat capacity is exactly $\tfrac{5}{2}R = 20.786\ \mathrm{J\,mol^{-1}\,K^{-1}}$ and constant. It is stored as a polynomial with only the leading term, which is not a shortcut but the correct form: a monatomic gas has no rotational or vibrational modes to excite.

Methanol uses a six-parameter variant with three hyperbolic terms:

$$C_p = A + B\left[\frac{C/2T}{\sinh(C/2T)}\right]^2 + D\left[\frac{E/2T}{\sinh(E/2T)}\right]^2 + F\left[\frac{G/2T}{\sinh(G/2T)}\right]^2$$

with $A = 33258$, $B = 36199$, $C = 1205.7$, $D = 1.5373 \times 10^7$, $E = 3212.2$, $F = -1.5318 \times 10^7$, $G = 3212.2$.

The third and fourth terms share a characteristic temperature and nearly cancel, $D + F = 5.5 \times 10^4$ against magnitudes of $1.5 \times 10^7$. This is retained as published rather than collapsed, because collapsing it would change the numbers being cited.

### Verification

Computed against NIST WebBook gas-phase values at three temperatures spanning the reference point, the synthesis loop and the reforming range.

| Species | 298 K | 800 K | 1300 K |
|---|---|---|---|
| $\mathrm{CO_2}$ | +0.33 % | −0.54 % | +0.20 % |
| $\mathrm{H_2}$ | −0.22 % | −0.16 % | +0.11 % |
| $\mathrm{CO}$ | −0.04 % | +0.07 % | −0.04 % |
| $\mathrm{H_2O}$ | −0.07 % | −0.05 % | +0.01 % |
| $\mathrm{CH_3OH}$ | +0.19 % | +0.18 % | +0.59 % |
| $\mathrm{CH_4}$ | −0.03 % | −0.95 % | −1.74 % |
| $\mathrm{N_2}$ | +0.09 % | −0.03 % | 0.00 % |
| $\mathrm{Ar}$ | exact | exact | exact |
| $\mathrm{O_2}$ | −0.18 % | −0.03 % | +0.02 % |

Worst deviation 1.74 %, methane at 1300 K. Checking three temperatures instead of one verifies the curvature of the correlation across the range actually used, not only its value at the reference point.

The reference values live in a separate file from the correlation coefficients and come from a different source, so a transcription error in the coefficients cannot pass by comparing the code against itself.

## Vapour pressure

Three species condense in this process. They use the DIPPR-101 form, with $P$ in pascals:

$$\ln P^{\text{sat}} = A + \frac{B}{T} + C\ln T + D\,T^{E}$$

| Species | $A$ | $B$ | $C$ | $D$ | $E$ |
|---|---|---|---|---|---|
| $\mathrm{H_2O}$ | 73.649 | −7258.2 | −7.3037 | $4.1653\times10^{-6}$ | 2 |
| $\mathrm{CH_3OH}$ | 82.718 | −6904.5 | −8.8622 | $7.4664\times10^{-6}$ | 2 |
| $\mathrm{CO_2}$ | 47.0169 | −2839.0 | −3.86388 | $2.81\times10^{-16}$ | 6 |

The remaining six species are permanent gases at every condition in this model, with critical temperatures well below the coldest point in the flowsheet ($\mathrm{CH_4}$, $T_c = 190.56\ \mathrm{K}$, is the highest of them against a minimum process temperature of 308 K). They return a large sentinel that forces their equilibrium ratio toward the vapour phase.

### Verification

Two of the three checks are definitional, which is what makes them useful. The normal boiling point is the temperature at which vapour pressure equals one atmosphere, so the correlation must return exactly $101{,}325\ \mathrm{Pa}$ there, and the vapour pressure curve terminates at the critical point, so $P^{\text{sat}}(T_c)$ must equal $P_c$.

| Check | Computed | Expected | Deviation |
|---|---|---|---|
| Water at 373.15 K | 101,260 Pa | 101,325 Pa | −0.06 % |
| Methanol at 337.85 K | 101,990 Pa | 101,325 Pa | +0.65 % |
| $\mathrm{CO_2}$ at $T_c = 304.21$ K | 7.3828 MPa | 7.383 MPa | −0.003 % |

The third check is a cross-check between two independently populated tables, since $P_c$ comes from the critical constants and $P^{\text{sat}}$ from the vapour pressure correlation. Agreement to three parts in $10^5$ confirms the two tables are mutually consistent, not merely individually plausible.

`species.hpp` declares the property structures and the accessors. 

`species.cpp` holds the three tables and evaluates the vapour pressure correlation. 

`tests/reference_data.hpp` holds the reference values with their citations, and `tests/test_thermo.cpp` performs the comparison. 

The critical constants there are labelled a transcription check, because `species.cpp` cites the same handbooks; the normal boiling points are labelled independent, because a boiling point is a different fact about the substance than the DIPPR-101 coefficients that must reproduce it.
