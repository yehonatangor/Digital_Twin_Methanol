# Tri-Reforming Reactor

A four-reaction plug flow model over catalyst mass, using published intrinsic rate laws rather than an equilibrium assumption.

## Background: intrinsic kinetics and intraparticle diffusion

A rate law measured on crushed catalyst powder is an intrinsic rate. Every active site sees the bulk gas composition, because the diffusion path from the outside of a small particle to its interior is short compared with the reaction timescale. That is the rate the chemistry produces, and it is what a kinetics paper reports.

An industrial pellet is millimetres across, and reactants must diffuse inward through a porous network to reach interior sites. If reaction is fast relative to that diffusion, reactants are consumed near the outer shell and the core sits idle. The pellet then delivers less than the intrinsic rate would predict.

The correction is the effectiveness factor:

$$\eta = \frac{\text{observed rate for the pellet}}{\text{rate at bulk surface conditions}}$$

$\eta = 1$ means no transport limitation and the whole pellet is active. $\eta = 0.05$, as for several reactions here, means only about five percent of the intrinsic capacity is realised and the reaction is confined to a thin outer layer.

The governing group is the Thiele modulus, which compares reaction rate to diffusion rate:

$$\phi = L\sqrt{\frac{k}{D_{\text{eff}}}}$$

with $L$ a characteristic pellet dimension. Small $\phi$ gives $\eta \to 1$; large $\phi$ gives $\eta \approx 1/\phi$, so effectiveness falls inversely with pellet size. This is why reforming catalysts are made as rings, wagon wheels and other shapes with thin walls: the shape shortens the diffusion path while keeping the bed void fraction high enough to limit pressure drop.

Two consequences follow that matter for reading this document. First, an effectiveness factor is a property of the pellet and the operating condition, not of the chemistry, so a factor measured on one catalyst geometry does not transfer cleanly to another. Second, a strongly diffusion-limited reaction shows a reduced apparent activation energy, roughly half the intrinsic value, because the observed rate depends on the square root of the rate constant. Apparent kinetics fitted on industrial pellets therefore look different from intrinsic kinetics even when the chemistry is identical.

The values used here are 0.07, 0.06, 0.7 and 0.05 for the four reactions. Water gas shift is much less limited than the others because it is slower, so its Thiele modulus is smaller.

## Why a kinetic model, and why it needed sources from outside

Both papers that describe the tri-reforming front end of this process, Lim (2022) and Shi (2020), model the reformer as an equilibrium block. Neither publishes a rate law. A genuine plug flow model therefore needs kinetics from elsewhere, and two papers supply them.

Xu and Froment (1989), *AIChE Journal* 35(1), 88 to 96, give intrinsic rate equations for steam reforming, water gas shift and the combined reaction over a nickel catalyst. These are the standard reference for methane reforming kinetics.

Arab Aboosadi, Jahanmiri and Rahimpour (2011), *Applied Energy* 88, 2691 to 2701, assemble those three with a combustion rate law and supply the effectiveness factors, the reactor configuration and the validation case. Their rate constants are Xu and Froment's divided by 3.6, which is the conversion from kmol per kilogram-hour to mol per kilogram-second, confirmed by direct arithmetic rather than assumed.

## The reaction set

Four reactions, indexed to match Aboosadi's own labelling.

$$\mathrm{CH_4} + \mathrm{H_2O} \rightleftharpoons \mathrm{CO} + 3\,\mathrm{H_2}
\qquad \Delta H_{298} = +205.9\ \mathrm{kJ\,mol^{-1}}$$

$$\mathrm{CH_4} + 2\,\mathrm{H_2O} \rightleftharpoons \mathrm{CO_2} + 4\,\mathrm{H_2}
\qquad \Delta H_{298} = +164.8\ \mathrm{kJ\,mol^{-1}}$$

$$\mathrm{CO} + \mathrm{H_2O} \rightleftharpoons \mathrm{CO_2} + \mathrm{H_2}
\qquad \Delta H_{298} = -41.2\ \mathrm{kJ\,mol^{-1}}$$

$$\mathrm{CH_4} + 2\,\mathrm{O_2} \rightarrow \mathrm{CO_2} + 2\,\mathrm{H_2O}
\qquad \Delta H_{298} = -802.6\ \mathrm{kJ\,mol^{-1}}$$

### Why dry reforming is not among them

$\mathrm{CH_4} + \mathrm{CO_2} \rightleftharpoons 2\mathrm{CO} + 2\mathrm{H_2}$ is the first reaction minus the third, so it is not independent and adding it would over-specify the system.

Cho et al. state this directly for their own set, noting the four reactions are not linearly independent and that one is a combination of the others, and Aboosadi makes the same observation. Both nonetheless carry four reactions, because the rate expressions were regressed individually and keeping all four preserves the fitted forms. That is a kinetic argument instead of a stoichiometric one. Dry reforming emerges from the combination instead of being fitted separately, and no invented or calibrated term is used anywhere.

## Rate expressions

Xu and Froment, Equation 3 form, with partial pressures in bar and rates in $\mathrm{mol\,kg_{cat}^{-1}\,s^{-1}}$.

$$R_1 = \frac{k_1}{p_{\mathrm{H_2}}^{2.5}}
\left(p_{\mathrm{CH_4}} p_{\mathrm{H_2O}} - \frac{p_{\mathrm{H_2}}^{3} p_{\mathrm{CO}}}{K_{\mathrm{I}}}\right) \Big/ \phi^{2}$$

$$R_2 = \frac{k_3}{p_{\mathrm{H_2}}^{3.5}}
\left(p_{\mathrm{CH_4}} p_{\mathrm{H_2O}}^{2} - \frac{p_{\mathrm{H_2}}^{4} p_{\mathrm{CO_2}}}{K_{\mathrm{II}}}\right) \Big/ \phi^{2}$$

$$R_3 = \frac{k_2}{p_{\mathrm{H_2}}}
\left(p_{\mathrm{CO}} p_{\mathrm{H_2O}} - \frac{p_{\mathrm{H_2}} p_{\mathrm{CO_2}}}{K_{\mathrm{III}}}\right) \Big/ \phi^{2}$$

sharing one adsorption denominator:

$$\phi = 1 + K_{\mathrm{CO}} p_{\mathrm{CO}} + K_{\mathrm{H_2}} p_{\mathrm{H_2}}
+ K_{\mathrm{CH_4}} p_{\mathrm{CH_4}} + K_{\mathrm{H_2O}} \frac{p_{\mathrm{H_2O}}}{p_{\mathrm{H_2}}}$$

The fractional hydrogen exponents are a consequence of the mechanism, in which dissociative hydrogen adsorption enters the rate-determining step, and not a fitting artefact.

## Parameters

Verified by rendering the source pages at 400 dpi and running optical character recognition, an extraction path independent of the PDF text layer.

Rate constants, Xu and Froment Table 6 preexponentials with Table 5 activation energies:

| Constant | $A$ | $E$ ($\mathrm{J\,mol^{-1}}$) | Units of $A$ |
|---|---|---|---|
| $k_1$, steam reforming | $4.225\times10^{15}$ | 240,100 | $\mathrm{kmol\,bar^{0.5}\,kg^{-1}\,h^{-1}}$ |
| $k_2$, shift | $1.955\times10^{6}$ | 67,130 | $\mathrm{kmol\,bar^{-1}\,kg^{-1}\,h^{-1}}$ |
| $k_3$, combined | $1.020\times10^{15}$ | 243,900 | $\mathrm{kmol\,bar^{0.5}\,kg^{-1}\,h^{-1}}$ |

Adsorption constants, van't Hoff form $K = A\exp(-\Delta H/RT)$:

| Species | $A$ | $\Delta H$ ($\mathrm{J\,mol^{-1}}$) |
|---|---|---|
| $\mathrm{CO}$ | $8.23\times10^{-5}$ | −70,650 |
| $\mathrm{H_2}$ | $6.12\times10^{-9}$ | −82,900 |
| $\mathrm{CH_4}$ | $6.65\times10^{-4}$ | −38,280 |
| $\mathrm{H_2O}$ | $1.77\times10^{5}$ | +88,680 |

Equilibrium correlations, Aboosadi Table 2:

$$K_{\mathrm{I}} = \exp\!\left(\frac{-26830}{T} + 30.114\right)\ \mathrm{bar^2},
\qquad
K_{\mathrm{III}} = \exp\!\left(\frac{4400}{T} - 4.036\right),
\qquad
K_{\mathrm{II}} = K_{\mathrm{I}} K_{\mathrm{III}}$$

These are used instead of values computed from Gibbs energy, for the same reason given in the methanol kinetics document: the rate laws were regressed against these correlations.

### Catalyst activity basis

Table 6 carries the footnote "Reference activity", and Xu and Froment state on page 94 that the coefficients must be multiplied by 2.246 for fresh catalyst.

This module does not apply that factor, deliberately. Aboosadi, the source of the rate-law structure and the validation case, also uses Table 6 as printed, so applying it here would break the one comparison this module is checked against. The effect was measured before the decision was made: multiplying all three reforming preexponentials by 2.246 moves methane conversion from 96.20 to
96.49 percent and every outlet mole fraction by under 0.15 percent, because at 1100 K the reactor is equilibrium-limited instead of kinetically limited. It would matter for a shorter bed or a lower inlet temperature.

## Combustion

Aboosadi Equation 10, Trimm and Lam kinetics with adsorption parameters refitted to nickel per De Smet et al.:

$$R_4 = \frac{k_{4a}\, p_{\mathrm{CH_4}} p_{\mathrm{O_2}}}{1 + K^C_{\mathrm{CH_4}} p_{\mathrm{CH_4}} + K^C_{\mathrm{O_2}} p_{\mathrm{O_2}}}
+ \frac{k_{4b}\, p_{\mathrm{CH_4}} p_{\mathrm{O_2}}}{1 + K^C_{\mathrm{CH_4}} p_{\mathrm{CH_4}} + K^C_{\mathrm{O_2}} p_{\mathrm{O_2}}}$$

with $k_{4a} = 8.11\times10^{5}$, $k_{4b} = 6.82\times10^{5}$, both at $E = 86{,}000\ \mathrm{J\,mol^{-1}}$, and adsorption terms $K^C_{\mathrm{CH_4}} = 1.26\times10^{-1}$ at −27,300 and $K^C_{\mathrm{O_2}} = 7.78\times10^{-7}$ at −92,800 $\mathrm{J\,mol^{-1}}$.

Both terms share one linear denominator, exactly as Aboosadi prints it. De Smet's original carries a squared denominator on the first term, and Aboosadi's own units for $k_{4a}$, $\mathrm{bar^{-2}}$, are consistent with that squared form, so Equation 10 as printed has most likely dropped an exponent.

The paper is followed instead of the original, because Aboosadi supplies the rate structure, the effectiveness factors and the validation case, and substituting a different combustion form would break that comparison. The difference was measured first: at 1100 K and 20 bar the denominator is 10.36, so squaring it halves the combustion rate, from 78.06 to 39.75 $\mathrm{mol\,kg_{cat}^{-1}\,s^{-1}}$. The whole-reactor effect is nil:

| Form | $X_{\mathrm{CH_4}}$ | $T_{\text{out}}$ | $\mathrm{H_2/CO}$ | $\mathrm{CO_2}$ | $\mathrm{H_2}$ |
|---|---|---|---|---|---|
| Linear, as coded | 0.9620 | 1143.25 K | 1.7629 | 17.74 % | 28.06 % |
| Squared, De Smet | 0.9619 | 1143.29 K | 1.7628 | 17.74 % | 28.05 % |

Oxygen is the limiting reagent and burns to completion either way. A slower combustion moves the reaction zone deeper into the bed while releasing the same heat. The choice would not be free in an oxygen-rich or short-bed case, where the combustion zone position governs the peak temperature.

## Effectiveness factors

Intraparticle transport limitation is handled by a constant factor per reaction, applied between the intrinsic rate and the species balance:

$$r_i = \sum_j \eta_j \nu_{ij} R_j$$

with $\eta_1 = 0.07$, $\eta_2 = 0.06$, $\eta_3 = 0.7$, $\eta_4 = 0.05$ (Aboosadi, from De Groote and Froment 1996).

The reforming reactions are strongly diffusion-limited, running at 6 to 7 percent of their intrinsic rate, while the shift is much less so at 70 percent. That contrast is the physical signature of a fast reaction on a large pellet. These factors come from a partial oxidation study instead of this exact catalyst, which is Aboosadi's own modelling choice carried through unchanged.

## Integration

The same fourth-order Runge-Kutta scheme over catalyst mass as the methanol reactor, with the same step-size rule and the same measured non-negativity clamp. Both are documented in the reactor integration document.

The target step size differs. Methanol synthesis uses 2 grams of catalyst per step; the tri-reformer uses 1.4 kilograms, because the Aboosadi vessel holds about 5,600 kg where a methanol tube holds single-digit kilograms. A step size appropriate for one is absurd for the other in both directions.

This module is where the clamp guard was shown to matter. Oxygen is consumed within the first few percent of the bed, so it is the species a coarse grid drives negative first, and clamping it creates oxygen. At a 100 tonne charge with too coarse a grid integration overshoot can measure a 4.3 percent oxygen surplus while carbon and hydrogen still closed to $10^{-14}$ and the total mass check passed.

Pressure drop defaults to none, which is not a shortcut. Aboosadi's model assumption nine states that pressure is constant along the reactor.

## Verification

Rate and adsorption constants at the reference temperatures, against Xu and Froment Table 5. Each computed value falls inside the paper's own published confidence interval:

| Constant | Lower limit | Computed | Upper limit |
|---|---|---|---|
| $k_{1,648}$ | $1.64\times10^{-4}$ | $1.871\times10^{-4}$ | $2.05\times10^{-4}$ |
| $k_{2,648}$ | 6.915 | 7.585 | 8.200 |
| $k_{3,648}$ | $1.78\times10^{-5}$ | $2.231\times10^{-5}$ | $2.60\times10^{-5}$ |
| $K_{\mathrm{CO},648}$ | 37.17 | 40.77 | 44.65 |
| $K_{\mathrm{H_2},648}$ | 0.0059 | 0.0295 | 0.0533 |
| $K_{\mathrm{CH_4},823}$ | 0.1371 | 0.1788 | 0.2211 |
| $K_{\mathrm{H_2O},823}$ | 0.0317 | 0.4166 | 0.5032 |

Reaction enthalpies against Aboosadi Equations 1 and 4 to 6, all within
0.2 percent, with the internal consistency check that the combined reaction enthalpy equals steam reforming plus shift exactly.

Outlet composition against Aboosadi Table 8, at their optimised feed of 1100 K and 20 bar:

| | $X_{\mathrm{CH_4}}$ | $\mathrm{CO_2}$ | $\mathrm{CO}$ | $\mathrm{H_2}$ | $\mathrm{CH_4}$ | $\mathrm{H_2O}$ |
|---|---|---|---|---|---|---|
| Computed | 96.2 % | 17.74 | 15.91 | 28.06 | 0.559 | 37.73 |
| Aboosadi | 99.6 % | 18.18 | 17.54 | 22.13 | 0.06 | 40.37 |

Tolerance here is deliberately loose. Aboosadi solves separate gas and solid phase energy balances coupled by an external heat transfer coefficient; this module uses a single bulk temperature, the same simplification the methanol reactor makes. Exact reproduction was not the target and is not claimed.

Numerical properties: carbon, hydrogen and oxygen balances close to $10^{-12}$, and results are unchanged under fourfold grid refinement at both the validation charge and a charge eighteen times larger.

## Validity

The Aboosadi case runs at 1100 K and 20 bar. Xu and Froment Table 1 gives their steam reforming experiments at 3.0 to 15.0 bar and 773, 798, 823 and 848 K, read directly from the page.

Aboosadi extrapolates the kinetics on both axes, and this module follows them. Temperature is 336 to 394 percent beyond the fitted window measured against its width, and pressure 42 percent beyond. Two viscosity correlations are also exceeded, water above 1073 K and methane above 1000 K.

None of this is hidden. The ranges are encoded in the validity module and any design point that reaches them is flagged instead of extrapolated.

In the code. `front_end/trm_reactor.hpp` declares the reaction set, stoichiometry, kinetics parameters and effectiveness factors, with the full reasoning for the reaction-independence and combustion-denominator decisions. `trm_reactor.cpp` holds the rate laws, the species rates derived from the stoichiometry matrix, the energy balance and the integration loop.