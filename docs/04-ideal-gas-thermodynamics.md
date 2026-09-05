# Ideal-Gas Thermodynamics

Enthalpy, entropy and Gibbs energy from heat capacity and formation data, and the equilibrium constants that follow from them.

## Background: the thermochemical bookkeeping

Absolute enthalpy has no meaning. Only differences are measurable, so thermochemistry fixes an arbitrary zero and works from it. The convention is that every element in its standard state has zero enthalpy of formation at 298.15 K and 1 bar. Hydrogen gas, nitrogen gas, graphite and liquid bromine are all assigned zero. The enthalpy of formation of a compound is then the enthalpy change for making one mole of it from those elements.

This convention makes reaction enthalpy a subtraction:

$$\Delta H_r = \sum_i \nu_i \Delta H_{f,i}^\circ$$

with $\nu$ positive for products and negative for reactants. Hess's law guarantees the result is path independent, which is what allows a reaction that has never been run in a calorimeter to be evaluated from tabulated compounds.

Entropy is different. The third law fixes a genuine absolute zero at $T = 0$ for a perfect crystal, so $S^\circ$ values in the table below are absolute rather than relative, and elements have non-zero entropies. The asymmetry is easy to miss: enthalpies of formation for elements are zero, entropies are not.

### Why Gibbs energy governs equilibrium

A reaction proceeds spontaneously if it lowers the total Gibbs energy at constant temperature and pressure:

$$\Delta G = \Delta H - T\Delta S$$

The two terms compete. $\Delta H < 0$ favours reaction because energy is released to the surroundings; $\Delta S > 0$ favours it because the system becomes more disordered. Temperature sets the balance. Methanol synthesis is exothermic and reduces mole count, so it is enthalpy-favoured and entropy-disfavoured, and it is run as cold as the kinetics allow. Steam reforming is the reverse case, endothermic with an increase in moles, which is why it needs 800 $^\circ\mathrm{C}$ and above.

At equilibrium the Gibbs energy reaches its minimum, and thermodynamics gives:

$$\Delta G_r(T) = -RT \ln K$$

so that

$$K = \exp\!\left(-\frac{\Delta G_r(T)}{RT}\right)$$

The equilibrium constant is therefore not an independent piece of data. It follows from the same formation table used for the energy balance, which is why this project computes $K$ rather than storing correlations for it wherever possible. The one place that rule is broken is the methanol kinetics, where the published rate law was regressed against a specific $K$ correlation and substituting a different value would unbalance the forward and reverse terms.

A useful consequence is the van 't Hoff relation, obtained by differentiating:

$$\frac{d\ln K}{d(1/T)} = -\frac{\Delta H_r}{R}$$

An exothermic reaction has $K$ falling with temperature. This is the thermodynamic statement of Le Chatelier's principle and it is the central tension in methanol synthesis: raising temperature speeds the kinetics and lowers the attainable conversion.

## Formation properties

Standard enthalpy of formation and standard entropy at $T_{\text{ref}} = 298.15\ \mathrm{K}$ and 1 bar, gas phase throughout.

| Species | $\Delta H_f^\circ$ ($\mathrm{kJ\,mol^{-1}}$) | $S^\circ$ ($\mathrm{J\,mol^{-1}\,K^{-1}}$) |
|---|---|---|
| $\mathrm{CO_2}$ | −393.51 | 213.78 |
| $\mathrm{H_2}$ | 0 | 130.68 |
| $\mathrm{CO}$ | −110.53 | 197.66 |
| $\mathrm{H_2O}$ | −241.83 | 188.84 |
| $\mathrm{CH_3OH}$ | −201.00 | 239.90 |
| $\mathrm{CH_4}$ | −74.60 | 186.25 |
| $\mathrm{N_2}$ | 0 | 191.60 |
| $\mathrm{Ar}$ | 0 | 154.85 |
| $\mathrm{O_2}$ | 0 | 205.15 |

Source: NIST Chemistry WebBook.

Two points on the water and methanol entries carry consequences later.

$\mathrm{H_2O}$ is the **gas-phase** value. The liquid-phase formation enthalpy is −285.83 $\mathrm{kJ\,mol^{-1}}$, and the 44 $\mathrm{kJ\,mol^{-1}}$ difference is the enthalpy of vaporisation. A gas-phase reactor energy balance must use the gas-phase value.

$\mathrm{CH_3OH}$ is likewise the gas-phase value. Van-Dal reports the methanol synthesis enthalpy as −87 $\mathrm{kJ\,mol^{-1}}$, which is on a liquid methanol basis and is not usable in a gas-phase balance. The gas-basis value is −49 $\mathrm{kJ\,mol^{-1}}$.

## Temperature dependence

Enthalpy at any temperature follows from integrating heat capacity from the reference point, which is Kirchhoff's relation:

$$H_i(T) = \Delta H_{f,i}^\circ + \int_{T_{\text{ref}}}^{T} C_{p,i}(T')\,dT'$$

$$S_i(T) = S_i^\circ + \int_{T_{\text{ref}}}^{T} \frac{C_{p,i}(T')}{T'}\,dT'$$

Both integrals are evaluated analytically rather than numerically, because the DIPPR-107 form admits closed antiderivatives. For the term $B\left[(C/T)/\sinh(C/T)\right]^2$:

$$\int B\left[\frac{C/T}{\sinh(C/T)}\right]^2 dT = \frac{BC}{\tanh(C/T)}$$

$$\int \frac{B}{T}\left[\frac{C/T}{\sinh(C/T)}\right]^2 dT = B\left[\frac{C/T}{\tanh(C/T)} - \ln\sinh\!\left(\frac{C}{T}\right)\right]$$

and for the cosine-hyperbolic term $D\left[(E/T)/\cosh(E/T)\right]^2$:

$$\int D\left[\frac{E/T}{\cosh(E/T)}\right]^2 dT = -DE\tanh\!\left(\frac{E}{T}\right)$$

$$\int \frac{D}{T}\left[\frac{E/T}{\cosh(E/T)}\right]^2 dT = -D\left[\frac{E}{T}\tanh\!\left(\frac{E}{T}\right) - \ln\cosh\!\left(\frac{E}{T}\right)\right]$$

Analytic integration matters for the energy balance, where enthalpy is evaluated at every step of a Runge-Kutta integration with thousands of steps. A numerical quadrature nested inside that loop would dominate the cost and introduce its own error.

### Overflow in the logarithmic terms

At low temperature the argument $u = C/T$ becomes large and $\sinh u$ overflows double precision near $u \approx 710$. The entropy integral needs $\ln \sinh u$, not $\sinh u$ itself, so the identity

$$\ln\sinh u = u - \ln 2 + \ln\!\left(1 - e^{-2u}\right)$$

is used for $u > 20$, evaluated with `log1p` to preserve precision when $e^{-2u}$ is small. The equivalent identity for $\ln\cosh u$ replaces the minus with a plus.

## Reaction properties

For a reaction with stoichiometric coefficients $\nu_i$, negative for reactants:

$$\Delta H_r(T) = \sum_i \nu_i H_i(T), \qquad \Delta S_r(T) = \sum_i \nu_i S_i(T)$$

$$\Delta G_r(T) = \Delta H_r(T) - T\,\Delta S_r(T)$$

Both terms carry their full temperature dependence. Treating $\Delta H_r$ as constant at its 298 K value is a common simplification and is wrong by a significant margin over the range this model uses. For steam reforming between 298 K and 1100 K the change is several $\mathrm{kJ\,mol^{-1}}$.

### Verification

Reaction enthalpies at 298.15 K against two independent papers.

| Reaction | Computed | Lim (2022) | Shi (2020) |
|---|---|---|---|
| $\mathrm{CH_4} + \mathrm{H_2O} \rightarrow \mathrm{CO} + 3\mathrm{H_2}$ | +205.9 | +206.3 | +206.8 |
| $\mathrm{CH_4} + \mathrm{CO_2} \rightarrow 2\mathrm{CO} + 2\mathrm{H_2}$ | +247.05 | +247.3 | +247.3 |
| $\mathrm{CH_4} + \tfrac12\mathrm{O_2} \rightarrow \mathrm{CO} + 2\mathrm{H_2}$ | −35.93 | −35.6 | −35.6 |

in $\mathrm{kJ\,mol^{-1}}$. The residual differences trace to the methane formation enthalpy, where −74.60 and −74.85 $\mathrm{kJ\,mol^{-1}}$ both appear in the literature.

For the two synthesis reactions:

| Reaction | Computed | Published |
|---|---|---|
| $\mathrm{CO_2} + 3\mathrm{H_2} \rightarrow \mathrm{CH_3OH} + \mathrm{H_2O}$ | −49.32 | −49 (Shi, gas basis) |
| $\mathrm{CO_2} + \mathrm{H_2} \rightarrow \mathrm{CO} + \mathrm{H_2O}$ | +41.15 | +41 (Van-Dal) |

Reproducing the gas-basis −49 rather than the liquid-basis −87 confirms the formation table is being used on the correct phase.

## Equilibrium constants from Gibbs energy

$$\ln K = -\frac{\Delta G_r(T)}{RT}, \qquad K = \exp\!\left(-\frac{\Delta G_r(T)}{RT}\right)$$

$K$ is dimensionless and referenced to the standard state of 1 bar, because the tabulated entropies are. A reaction quotient compared against it must therefore use partial pressures in bar:

$$K = \prod_i \left(\frac{p_i}{p^\circ}\right)^{\nu_i}, \qquad p^\circ = 1\ \mathrm{bar}$$

For a reaction with $\Delta n = \sum_i \nu_i \ne 0$, using pascals instead introduces a factor of $(10^5)^{\Delta n}$. Steam reforming has $\Delta n = 2$, so the error would be $10^{10}$. This is the reason the pressure convention is stated at every interface.

## Two routes to the same constant

The model computes equilibrium constants two ways, and the choice is made per use and not globally.

From Gibbs energy, as above. Used by the reforming equilibrium solver. It is thermodynamically consistent by construction and requires no fitted correlation, but it inherits whatever error is present in the formation data.

From a published correlation. Used by the kinetic rate expressions, for example the Graaf correlation for methanol synthesis:

$$\log_{10} K_{eq} = \frac{3066}{T} - 10.592$$

A rate law was regressed against a specific equilibrium correlation. The approach to equilibrium in the rate expression, the bracket that vanishes at equilibrium, was fitted with that $K$ in place. Substituting a Gibbs-energy value decouples the forward and reverse terms from what the authors actually fitted, and the rate no longer vanishes exactly at the equilibrium the correlation defines.

So a rate law keeps the correlation it was fitted with, and a standalone
equilibrium calculation uses Gibbs energy. Mixing them is the error to avoid.

`thermo.hpp` declares the interface. 

`thermo.cpp` holds the formation table, the analytic integrals, the overflow-safe logarithms and the reaction-property functions. 

Verification is in `tests/test_thermo.cpp` against `tests/reference_data.hpp`, which labels each reference value. 

The reaction enthalpies above are the independent ones: Lim, Shi and Van-Dal publish them and none of them supplied this project's formation table, so agreement tests the table, the Kirchhoff integration and the stoichiometry together. 

The formation enthalpies themselves are a transcription check against the same NIST-JANAF and CRC data the code cites.

No independent ideal-gas $C_p$ reference is held. 

`species.cpp` takes its DIPPR-107 coefficients from Perry's 8th ed. Table 2-155 and this project has no second source for $C_p$, so the tests assert only what holds regardless of the coefficients: positivity for all nine species, the rise with temperature for a polyatomic, and a flat $C_p$ for monatomic argon.
