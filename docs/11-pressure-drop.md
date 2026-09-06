# Pressure Drop

The Ergun equation in a packed bed, its transformation to the catalyst-mass coordinate, and the bed geometries used.

## Background: flow through a packed bed

A packed bed is a vessel filled with catalyst pellets, and gas flows through the void space between them. That void space is a tortuous network of irregular channels, not a pipe, so the friction factor correlations for pipe flow do not apply directly.

The standard treatment starts by modelling the bed as a bundle of parallel channels of equivalent hydraulic diameter, then correcting for the tortuous path length. Two limits follow.

At low flow, viscous drag on the channel walls dominates and pressure drop is linear in velocity. This is Darcy's law, the same relation that governs flow through soil and rock, and for a packed bed it takes the Blake-Kozeny form.

At high flow, inertial losses dominate: the gas repeatedly accelerates into a constriction and decelerates in the wake behind a pellet, dissipating kinetic energy each time. Pressure drop then goes as velocity squared, which is the Burke-Plummer form.

Ergun's contribution was to observe that the two mechanisms operate together across the industrial range and to add them, fitting the two coefficients 150 and 1.75 to a large body of data. The result covers the whole flow regime with one expression, which is why it is still the standard fifty years later.

### The role of void fraction

Void fraction $\varepsilon$ is the fraction of bed volume that is empty, and it enters both terms with high powers: $(1-\varepsilon)^2/\varepsilon^3$ for the viscous term and $(1-\varepsilon)/\varepsilon^3$ for the inertial term. Dropping $\varepsilon$ from 0.40 to 0.35 raises the viscous term by 74 percent, because tighter packing narrows the channels and lengthens the path at the same time.

Randomly packed spheres give $\varepsilon \approx 0.36$ to 0.40. Rings and other shaped supports give 0.45 to 0.65, which is why they are chosen when pressure drop rather than surface area is the constraint. The tri-reforming catalyst in this project is a ten-hole ring for that reason.

The strong dependence also means that an unmeasured void fraction is a genuine source of uncertainty in any pressure drop estimate, which is the situation the tri-reformer bed is in and which is flagged where it arises.

## The equation

Ergun (1952), in the mass-flux form:

$$-\frac{dP}{dz} =
\underbrace{\frac{150\,\mu\,(1-\varepsilon)^{2}\,G}{\rho\,d_p^{2}\,\varepsilon^{3}}}_{\text{viscous}}
+
\underbrace{\frac{1.75\,(1-\varepsilon)\,G^{2}}{\rho\,d_p\,\varepsilon^{3}}}_{\text{inertial}}$$

with $G = \rho u_s$ the superficial mass flux in $\mathrm{kg\,m^{-2}\,s^{-1}}$, $\varepsilon$ the void fraction, $d_p$ the particle diameter, $\mu$ the gas viscosity and $\rho$ the gas density.

The mass-flux form is used rather than the velocity form because $G$ is constant down the bed even as density changes with temperature, pressure and mole number. Writing the equation in terms of $u_s$ requires recomputing velocity at every step from a density that is itself changing, which introduces an unnecessary coupling.

## Which term dominates

The ratio of the two terms is proportional to the particle Reynolds number:

$$Re_p = \frac{d_p\,G}{\mu}$$

Below roughly $Re_p = 10$ the viscous term dominates. Above roughly 1000 the inertial term does. Between them both contribute, which is the regime most industrial beds occupy.

For the Van-Dal laboratory bed at its stated feed, $Re_p \approx 25$, so the viscous term leads but the inertial term is not negligible. Both are retained everywhere instead of selecting a branch, since the full expression costs nothing extra.

## Transformation to catalyst mass

The reactor integrates over catalyst mass, so the pressure gradient must be expressed per unit mass instead of per unit length. The two coordinates are related through the bed:

$$W = \rho_{\text{bulk}}\,A_c\,z \qquad\Longrightarrow\qquad
\frac{dP}{dW} = \frac{1}{\rho_{\text{bulk}}\,A_c}\,\frac{dP}{dz}$$

where $A_c$ is the tube cross-sectional area and

$$\rho_{\text{bulk}} = \rho_p\,(1 - \varepsilon)$$

is the bed-average catalyst density, not the particle density.

### The bulk density distinction

Catalyst data sheets quote particle density, meaning mass per unit volume of pellet. The bed contains void space, so the catalyst mass per unit of reactor volume is lower by the factor $(1-\varepsilon)$.

Using $\rho_p$ where $\rho_{\text{bulk}}$ belongs overstates the catalyst inventory by $1/(1-\varepsilon)$, which is a factor of two at $\varepsilon = 0.5$. Since bed length, catalyst mass and residence time are all computed from this quantity, the error propagates into conversion as well as pressure drop.

## Bed geometries

Three presets, differing in what each source actually publishes.

### Van-Dal laboratory bed

Van-Dal Appendix A and Table A.2: tube inner diameter 0.016 m, bed length
0.15 m, catalyst mass 34.8 g, void fraction 0.5, particle density 1775 $\mathrm{kg\,m^{-3}}$, particle diameter 0.5 mm, single tube, adiabatic.

These five values are mutually inconsistent. With $\rho_{\text{bulk}} = 1775 \times 0.5 = 887.5\ \mathrm{kg\,m^{-3}}$ and $A_c = \tfrac{\pi}{4}(0.016)^2 = 2.011\times10^{-4}\ \mathrm{m^2}$, a mass of
34.8 g implies

$$L = \frac{0.0348}{887.5 \times 2.011\times10^{-4}} = 0.195\ \mathrm{m}$$

against the stated 0.15 m. Equivalently, 0.15 m implies 26.8 g against the stated 34.8 g.

The bed is over-determined by one value and the source does not say which to drop. Two presets are provided instead of choosing:

| Preset | Primary | Derived |
|---|---|---|
| mass primary | 34.8 g, $\varepsilon = 0.5$ | $L = 0.195$ m |
| length primary | $L = 0.15$ m, $\varepsilon = 0.5$ | 26.8 g |

The mass-primary form is the default, because catalyst mass is the quantity the rate expression is normalised against and is therefore the more consequential of the two.

### Shi boiling water reactor

Shi et al. Section 3.1: 2700 tubes, 0.035 m inner diameter, 7.0 m length, 5 mm spherical pellets, bulk density 1140 $\mathrm{kg\,m^{-3}}$.

The void fraction is not stated directly but follows from the text, which gives the catalyst as filling 60 % of the reactor volume:

$$\varepsilon = 0.40, \qquad \rho_p = \frac{1140}{0.60} = 1900\ \mathrm{kg\,m^{-3}}$$

### Industrial synthesis bed

Van-Dal Table 2 publishes the industrial catalyst, void fraction 0.4, particle diameter 5.5 mm, particle density 1775 $\mathrm{kg\,m^{-3}}$, total charge 44,500 kg, but no tube count, tube diameter or bed length anywhere in the paper.

That gap cannot be closed by reading more carefully. This project takes Shi's tube diameter and length, which are the quantities Van-Dal genuinely does not publish, and derives the tube count from Van-Dal's own stated charge:

$$n_{\text{tubes}} = \frac{44{,}500}{\rho_{\text{bulk}} A_c L}
= \frac{44{,}500}{7.173} = 6204$$

### Why the count is derived instead of adopted

Adopting Shi's 2700 tubes wholesale alongside Van-Dal's 44,500 kg catalyst mass creates a physical contradiction, undersizing the bed by a factor of 2.3.

The error was invisible for as long as the flowsheet forced the knockout drum to a fixed loop pressure, because the computed pressure drop was discarded before anything downstream could see it. Once the drum was moved to the real reactor outlet pressure, the undersized bundle produced a drop of tens of bar, the gas density collapsed, and the recycle loop stopped converging.

Deriving the count uses one more published number and one fewer substitution, so the provenance improves at the same time as the physics. The consequences at the design point:

| | 2700 tubes | 6204 tubes |
|---|---|---|
| Catalyst charge | 19,366 kg | 44,499 kg |
| Single-pass pressure drop | 1.004 bar | 0.194 bar |
| Single-pass $\mathrm{CO_2}$ conversion | 15.2 % | 24.0 % |
| Recycle loop | does not converge | converges at 70 % recycle |

Any result depending on bed sizing inherits this decision. The tube dimensions remain Shi's and remain a declared substitution.

## Pressure drop models

Three options, because two of the source cases specify different treatments.

| Model | Behaviour |
|---|---|
| `None` | $dP/dW = 0$, isobaric |
| `Ergun` | the full equation above |
| `ConstantTotal` | a fixed total drop distributed uniformly over the bed |

`ConstantTotal` exists because Mucci states that a 1 bar drop was assumed for each reactor stage. Using it there is not a simplification introduced here; it reproduces what the source actually modelled.

`None` is the default for the tri-reformer, because Aboosadi's own model assumption nine states that pressure is constant along the reactor.

## Verification

Hand-derived case, chosen so the arithmetic can be checked by inspection: $\mu = 2\times10^{-5}$ Pa s, $\rho = 10$ $\mathrm{kg\,m^{-3}}$, $G = 1$ $\mathrm{kg\,m^{-2}\,s^{-1}}$, $\varepsilon = 0.5$, $d_p = 0.5$ mm.

Viscous term:

$$\frac{150 \times 2\times10^{-5} \times 0.25 \times 1}{10 \times (5\times10^{-4})^2 \times 0.125}
= 240\ \mathrm{Pa\,m^{-1}}$$

Inertial term:

$$\frac{1.75 \times 0.5 \times 1}{10 \times 5\times10^{-4} \times 0.125}
= 1400 \times 0.16785 = 235\ \mathrm{Pa\,m^{-1}}$$

Total $-475\ \mathrm{Pa\,m^{-1}}$, with $Re_p = 25$. Both reproduced exactly.

Additional checks: bulk density from particle density and void fraction, catalyst mass from bed geometry against both Van-Dal presets, and a round trip through the $dP/dz$ to $dP/dW$ transformation and back.

### What the laboratory case does not validate

At the Van-Dal laboratory conditions the total pressure drop is on the order of tens of pascals against a 50 bar operating pressure, a relative drop below $10^{-5}$. Reproducing Van-Dal's conversion profile therefore validates the kinetics and the energy balance but says nothing about the Ergun implementation, because pressure is effectively constant either way.

The order-of-magnitude anchor for pressure drop is instead Mucci's 1 bar per stage on an industrial bed, which is what the `ConstantTotal` model reproduces directly.

In the code. `ergun.hpp` declares the bed geometry type, the three pressure drop models and the presets. `ergun.cpp` holds the equation, the coordinate transformation, the geometry presets with the over-determination note and a diagnostic report.