# Conventions

The unit system, the composition representation, and the two places where a
quantity changes basis.

## Background: stream representation in process simulation

A process stream is fully specified by its component flows, its temperature, its pressure and its phase state. This is because of the phase rule: for a single-phase stream of $C$ components, $C + 2$ intensive variables fix the thermodynamic state, and one extensive variable fixes the scale. Anything else, such as density, enthalpy, viscosity or dew point, is a computed property.

Commercial simulators follow the same principle. Aspen Plus and similar tools carry component molar flows plus two state variables in the stream structure and compute the rest on demand through a property method. A simulator that stored enthalpy alongside temperature would have two representations of the same information that could drift apart after any edit.

The practical rule this leads to is that a model should store the smallest set of quantities from which everything else follows, and derive the remainder every time it is needed. The cost is recomputation. The benefit is that internal inconsistency becomes structurally impossible meaning the test has nothing it needs to look for.

## Why molar flow

A stream can be represented by molar flow, mass flow, mole fraction or mass fraction. Only one of these can be primary without introducing a consistency problem, because the others are derived from it and round-trip conversion is not exact in floating point.

Molar flow is primary here for three reasons.

1. Reaction stoichiometry is molar. 
    
    A mole balance over a reactor is $dF_i/dW = \sum_j \nu_{ij} r_j$ with integer or half-integer $\nu$. Writing the same balance on a mass basis requires multiplying through by molar mass at every term and the stoichiometric coefficients stop being exact.

2. Mixing is additive in moles.
    
    Combining two streams sums the molar flows component by component. Mole fractions do not add.

3. Conservation is checkable. 
    
    Element balances are linear in molar flow, so carbon, hydrogen and oxygen can be summed directly and compared against the inlet. This is used as a test in the reactor modules.

Mass quantities are computed on demand:

$$\dot m = \sum_i F_i M_i, \qquad y_i = \frac{F_i}{\sum_j F_j}$$

`Stream` stores a map from species to molar flow, plus temperature, pressure and a phase label.

## Units

| Quantity | Internal unit | Notes |
|---|---|---|
| Molar flow | $\mathrm{mol\,s^{-1}}$ | |
| Mass flow | $\mathrm{kg\,s^{-1}}$ | derived |
| Molar mass | $\mathrm{g\,mol^{-1}}$ in the species table | converted at use |
| Temperature | $\mathrm{K}$ | $^\circ\mathrm{C}$ only in preset constructors |
| Pressure | $\mathrm{Pa}$ | bar at kinetic interfaces, see below |
| Catalyst mass | $\mathrm{kg}$ per tube | |
| Reaction rate | $\mathrm{mol\,kg_{cat}^{-1}\,s^{-1}}$ | |
| Energy | $\mathrm{J}$, $\mathrm{W}$ | MW only in reporting |
| Cost | 2001 USD | escalated at the reporting boundary |

Species and units are set in upright math throughout these chapters, so carbon dioxide is written $\mathrm{CO_2}$ and a rate is $\mathrm{mol\,kg_{cat}^{-1}\,s^{-1}}$. Italic is reserved for variables, so $\mathrm{K}$ is the kelvin and $K$ is an
equilibrium constant.

The gas constant is $R = 8.314462618\ \mathrm{J\,mol^{-1}\,K^{-1}}$, the CODATA value, defined once in `units.hpp` and used everywhere. Compile-time assertions enforce exact round-trip fidelity for all unit conversions.

The molar mass table stores grams per mole because that is how it is published. Every consumer divides by 1000 at the point of use instead of storing a second table in kilograms, so there is one number per species.

## The two basis changes

Most unit errors in process models occur where a quantity crosses between two conventions. There are two such crossings here, and both are isolated to a single function.

### Pascals to bar

Published kinetic correlations are almost always fitted with partial pressures in bar. The Vanden Bussche and Froment rate expressions and the Xu and Froment rate expressions both are. The rest of the model works in pascals.

The conversion happens in one function per kinetics module and nowhere else:

$$p_i\ [\mathrm{bar}] = y_i \cdot \frac{P\ [\mathrm{Pa}]}{10^5}$$

Placing this at a single boundary means a unit error either affects every rate or none, which is a failure mode that shows up immediately in a validation case. Scattering the conversion through the rate expressions would allow one term to be wrong while the others are right, which does not.

The same applies to equilibrium constants computed from Gibbs energy. Standard entropies are tabulated at a reference pressure of 1 bar, so an equilibrium constant derived from them is dimensionless with respect to 1 bar. Partial pressures entering a reaction quotient must therefore be in bar. For a reaction with $\Delta n \ne 0$ the error from using pascals is a factor of $(10^5)^{\Delta n}$, which for steam reforming is $10^{10}$.

### Per tube to plant

Reactor molar flows and catalyst mass are per tube. A multitubular reactor with 2700 tubes is modelled as one tube carrying $1/2700$ of the flow.

This is the natural basis because the rate expression is per kilogram of catalyst and the pressure drop correlation is per unit of cross-sectional area. Both are tube-local quantities, and neither has a plant-scale form.

Scaling happens at the flowsheet boundary:

$$F_i^{\text{tube}} = \frac{F_i^{\text{plant}}}{n_{\text{tubes}}}, \qquad 
F_i^{\text{plant, out}} = n_{\text{tubes}} \cdot F_i^{\text{tube, out}}$$

Conversion, selectivity and temperature are ratios or intensive quantities, so they are the same on either basis and need no scaling.

## Catalyst mass as the integration coordinate

The reactor is integrated over catalyst mass $W$ rather than axial position $z$.

The two are related by

$$W = \rho_{\text{bulk}} A_c z, \qquad \rho_{\text{bulk}} = \rho_p (1 - \varepsilon)$$

where $A_c$ is the tube cross-section, $\rho_p$ the particle density and $\varepsilon$ the void fraction.

Mass is preferred because the rate expression is already per unit catalyst mass, so the mole balance in $W$ needs no geometric factor:

$$\frac{dF_i}{dW} = \sum_j \nu_{ij}\, r_j$$

The equivalent balance in $z$ carries $\rho_{\text{bulk}} A_c$ on the right hand side, which introduces bed geometry into a relation that is otherwise pure chemistry. Pressure drop is computed per unit length, so that one term is transformed instead:

$$\frac{dP}{dW} = \frac{1}{\rho_{\text{bulk}} A_c}\,\frac{dP}{dz}$$

Axial position is recovered for plotting when a source publishes a profile against length.

## Bulk density and a common error

Catalyst data sheets quote particle density, meaning mass per unit volume of solid pellet. A packed bed contains void space, so the mass of catalyst per unit of reactor volume is smaller:

$$\rho_{\text{bulk}} = \rho_p (1 - \varepsilon)$$

Using the particle density as though it were a bed density overstates the catalyst inventory by $1/(1-\varepsilon)$, a factor of two at $\varepsilon = 0.5$. The distinction is applied consistently in the bed geometry type, and the presets record which quantity the source paper actually published.

## Reported versus internal

Reporting converts once, at the boundary, and never feeds a converted value back into a calculation.

| Reported as | Internal |
|---|---|
| $\mathrm{t\,h^{-1}}$ | $\mathrm{kg\,s^{-1}}$ |
| MW | W |
| bar | Pa |
| $^\circ\mathrm{C}$ | K |
| USD in a stated year | 2001 USD |

`units.hpp` defines the constants and conversions with compile-time checks. 

`stream.hpp` defines the stream type and the derived mass accessors. 

The pascal to bar boundary is `reaction_rates` in `reactor_core.cpp` and `trm_reaction_rates` in `trm_reactor.cpp`.

The per-tube scaling is in the flowsheet layer.
