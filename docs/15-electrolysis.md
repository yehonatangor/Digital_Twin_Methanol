# Electrolysis

Hydrogen production from electricity, with a load-dependent efficiency that makes flexible operation non-trivial.

## Background: water electrolysis

Splitting water is thermodynamically uphill. The Gibbs energy change at standard conditions is +237 kJ per mole of hydrogen, which sets the minimum electrical work, and the enthalpy change is +286 kJ, the difference being heat the cell can draw from its surroundings. Dividing by the charge transferred, $2F$, gives two reference voltages:

$$E_{\text{rev}} = \frac{\Delta G}{2F} = 1.23\ \mathrm{V}, \qquad
E_{\text{tn}} = \frac{\Delta H}{2F} = 1.48\ \mathrm{V}$$

The reversible voltage is the thermodynamic minimum. The thermoneutral voltage is where the cell neither absorbs nor rejects heat. Real cells operate at 1.8 to
2.0 V, and everything above 1.48 V becomes waste heat, which is why the cooling duty in this module is computed as the shortfall from unity efficiency.

The gap between 1.23 V and the operating voltage is overpotential, and it has three sources that scale differently with current:

- Activation overpotential, the kinetic barrier at each electrode, which grows logarithmically with current through the Tafel relation.
- Ohmic overpotential, resistance of the membrane and hardware, which grows linearly with current.
- Concentration overpotential, mass transport limitation at high current, which grows sharply as a limiting current is approached.

Because the ohmic term is linear in current while the hydrogen produced is also linear in current, efficiency falls as a stack is driven harder. That is the physical content of the polynomial fitted below, and it is why part-load operation is more efficient per unit of hydrogen than full-load operation. It is also why flexible operation against a variable electricity price is a real optimisation rather than a threshold rule.

### The three technologies

| | Alkaline | PEM | Solid oxide |
|---|---|---|---|
| Electrolyte | KOH solution | solid polymer | ceramic oxide |
| Temperature | 60 to 90 $^\circ\mathrm{C}$ | 50 to 80 $^\circ\mathrm{C}$ | 700 to 900 $^\circ\mathrm{C}$ |
| Delivery pressure | low, needs compression | 20 to 40 bar | low |
| Load range | 20 to 100 % | 5 to 120 % | limited |
| Ramp rate | minutes | seconds | hours |
| Maturity | commercial for a century | commercial | early |

PEM is modelled here because it dominates the source paper and because its wide turndown and fast ramp suit a plant following an electricity price. Its differentiating feature for this flowsheet is pressurised delivery: producing hydrogen at 30 bar rather than atmospheric removes a compression stage, at the cost of a small efficiency penalty that appears as the pressure term in the correlation below.

Solid oxide is the efficiency leader on paper, because at 800 $^\circ\mathrm{C}$ part of the energy input can be supplied as heat instead of electricity, but it cannot follow a fluctuating load and is not a candidate here.

## The unit

A proton exchange membrane water electrolyser splits water into hydrogen and oxygen:

$$\mathrm{H_2O} \rightarrow \mathrm{H_2} + \tfrac12\,\mathrm{O_2}$$

Both products are used. Hydrogen feeds methanol synthesis. Oxygen feeds the partial oxidation step in the tri-reforming front end, which is one of the reasons the two routes combine well: the reformer needs an oxygen supply that would otherwise require an air separation unit.

## Efficiency

Efficiency is not a constant. It depends on how hard each module is driven and on the delivery pressure, and Mucci et al. (2023) Table A.1 fits it as a polynomial on the lower heating value basis:

$$\eta_{\mathrm{PEM}} = a_{00} + a_{10} P_{\mathrm{mod}} + a_{20} P_{\mathrm{mod}}^{2} + a_{01} p_{\mathrm{PEM}}$$

with module power in MW and delivery pressure in bar.

| Coefficient | Value | Unit |
|---|---|---|
| $a_{00}$ | 0.813 | dimensionless |
| $a_{10}$ | $-1.010\times10^{-1}$ | $\mathrm{MW^{-1}}$ |
| $a_{20}$ | $+1.397\times10^{-2}$ | $\mathrm{MW^{-2}}$ |
| $a_{01}$ | $-3.118\times10^{-4}$ | $\mathrm{bar^{-1}}$ |

Fitted with $R^2 = 0.999$ over $P_{\mathrm{mod}}$ from 0.2 to 2.5 MW and $p_{\mathrm{PEM}}$ from 20 to 40 bar.

All four coefficients were re-verified directly from the source , including signs, because three are negative and two carry exponents, which is the pattern most vulnerable to transcription error.

### Why the shape matters

The power terms give a maximum. Efficiency falls with increasing load through $a_{10}$, then the positive quadratic term slows that fall at high load. A module run at 40 percent of its rating is not simply producing less hydrogen, it is producing hydrogen at a different cost per unit of electricity.

That is what makes flexible operation a real optimisation problem instead of a threshold rule. When electricity is cheap the naive response is to run flat out, but the efficiency penalty at full load partly offsets the cheaper power. Spread across more modules at lower individual load and the efficiency improves while the capital cost rises.

Pressure enters negatively, so delivering hydrogen at 40 bar costs efficiency relative to 20 bar. That trades against downstream compression duty, since hydrogen delivered at higher pressure needs less compression to reach the synthesis loop.

### Operating outside the window

The polynomial is a fit, not a physical law, and outside its window it is an extrapolation. Two guards apply.

The result carries `in_fit_range`, false when either variable leaves the fitted box, and the message says the efficiency was extrapolated.

If the polynomial returns a value outside $(0, 1)$ the call fails instead of returning it. A negative or greater-than-unity efficiency is not a marginal extrapolation, it is a violation of the first law, and passing it downstream would produce a hydrogen rate with no physical meaning.

## Hydrogen rate

$$\dot m_{\mathrm{H_2}} = \frac{\eta_{\mathrm{PEM}} \, P_{\mathrm{PEM}}}{\mathrm{LHV}_{\mathrm{H_2}}}$$

with power in MW, which is MJ per second, and lower heating value in MJ per kilogram.

### The heating value is computed, not tabulated

The lower heating value is the enthalpy released when hydrogen burns to gaseous water:

$$\mathrm{H_2} + \tfrac12\,\mathrm{O_2} \rightarrow \mathrm{H_2O(g)}$$

$$\mathrm{LHV} = \frac{-\Delta H_r(298.15\ \mathrm{K})}{M_{\mathrm{H_2}}}
= \frac{241{,}830}{2.016} = 119.96\ \mathrm{MJ\,kg^{-1}}$$

This is evaluated from the formation table at run time instead of stored as a constant. The gain is not convenience but consistency: the same formation data that drives the reactor energy balance also sets the electrolyser efficiency basis, so the two cannot disagree.

It also makes the lower against higher heating value distinction structural instead of a matter of remembering. The higher heating value uses liquid water at −285.83 $\mathrm{kJ\,mol^{-1}}$ and gives 141.8 $\mathrm{MJ\,kg^{-1}}$, an 18 percent difference. Writing the combustion reaction with gaseous water is what makes it the lower value, and the reaction is written once in the code where it can be read.

## Water and oxygen

From the stoichiometry, per mole of hydrogen: one mole of water consumed, half a mole of oxygen produced.

$$\dot m_{\mathrm{H_2O}} = \dot m_{\mathrm{H_2}} \frac{M_{\mathrm{H_2O}}}{M_{\mathrm{H_2}}} = 8.94\,\dot m_{\mathrm{H_2}}$$

$$\dot m_{\mathrm{O_2}} = \dot m_{\mathrm{H_2}} \frac{0.5\,M_{\mathrm{O_2}}}{M_{\mathrm{H_2}}} = 7.94\,\dot m_{\mathrm{H_2}}$$

Nearly nine kilograms of water per kilogram of hydrogen, which is why feedwater appears as an operating cost line, and nearly eight kilograms of oxygen, which is why the reformer's oxygen demand can plausibly be met from the byproduct.

## Power accounting

Three quantities, kept separate because they are used differently.

$$P_{\text{cooling}} = P_{\mathrm{PEM}}(1 - \eta_{\mathrm{PEM}}),
\qquad
P_{\text{aux}} = 0.05\,P_{\mathrm{PEM}},
\qquad
P_{\text{total}} = P_{\mathrm{PEM}} + P_{\text{aux}}$$

Cooling duty is what the stack does not convert to chemical energy, which follows from the definition of efficiency. Auxiliary load is Mucci's stated flat five percent for balance of plant.

The distinction matters for dispatch, since electricity is billed on $P_{\text{total}}$ while hydrogen is produced from $P_{\mathrm{PEM}}$.

## Modules

Rated power is divided across `n_modules` before the efficiency polynomial is evaluated, because the fit is per module instead of per plant. A 10 MW installation as five 2 MW modules and as twenty 0.5 MW modules are different operating points on the same curve.

## Verification

| Check | Result |
|---|---|
| Lower heating value from formation data | 119.96 $\mathrm{MJ\,kg^{-1}}$ |
| All four Table A.1 coefficients | verified against source, signs included |
| Fit window | 0.2 to 2.5 MW, 20 to 40 bar |
| Water to hydrogen mass ratio | 8.937 |
| Oxygen to hydrogen mass ratio | 7.937 |
| Efficiency outside $(0,1)$ | rejected instead of returned |
| Outside fit window | flagged, computed, message set |

In the code. `front_end/electrolyzer.hpp` declares the polynomial coefficients with their citation, the fit window and the result fields. `electrolyzer.cpp` computes the heating value from the formation table, applies the polynomial and enforces the physical bounds.