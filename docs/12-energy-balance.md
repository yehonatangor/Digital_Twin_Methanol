# Energy Balance

The gas-phase temperature equation along the bed, and the derivation of the cooling coefficient it depends on.

## Background: thermal behaviour of a catalytic bed

A reactor energy balance is a statement that the enthalpy released by reaction either raises the temperature of the flowing gas or leaves through the wall. Which of the two dominates determines the reactor type.

Adiabatic operation removes no heat. Temperature rises in proportion to conversion, and for a single reaction the adiabatic temperature rise is

$$\Delta T_{\text{ad}} = \frac{(-\Delta H_r)\,C_{A0}\,X}{\sum_i F_i C_{p,i}}$$

Adiabatic beds are simple, cheap and appropriate when the exotherm is modest or when the reaction is equilibrium-insensitive. They are also self-limiting for an exothermic equilibrium reaction: the bed heats until it reaches the equilibrium temperature for its current composition and stops. Adiabatic methanol converters are staged with intercooling between beds for this reason.

Cooled operation removes heat through a wall. The multitubular design puts catalyst in tubes and boiling water in the shell, which fixes the coolant temperature at the saturation temperature of the steam being raised. This gives a nearly isothermal bed and recovers the reaction heat as useful steam. It is the dominant industrial choice for methanol synthesis.

Two thermal risks drive the design. Hot spots form where the local rate is highest, usually near the inlet where reactant concentration is greatest, and they can exceed the average bed temperature by tens of kelvin. Thermal runaway occurs when the rate of heat generation, which is exponential in temperature through the Arrhenius term, outpaces the rate of removal, which is only linear in temperature difference. The stability condition is that the removal line be steeper than the generation curve at the operating point, a result usually presented as the van Heerden diagram.

For a copper catalyst the practical ceiling is sintering rather than runaway. Above roughly 300 $^\circ\mathrm{C}$ the copper crystallites coarsen and surface area is lost irreversibly, which is why the coolant is held near 245 $^\circ\mathrm{C}$ and why the aging model in this project is temperature-driven.

### Heat transfer in a packed tube

The overall coefficient $U$ links duty to driving force through $Q = UA\,\Delta T$. In a cooled catalytic tube it is the series combination of three resistances: the bed-side film, the tube wall, and the boiling coolant.

$$\frac{1}{U} = \frac{1}{h_{\text{bed}}} + \frac{t_{\text{wall}}}{k_{\text{wall}}} + \frac{1}{h_{\text{boil}}}$$

The boiling side is usually a small resistance, since nucleate boiling gives coefficients in the thousands of $\mathrm{W\,m^{-2}\,K^{-1}}$, and a steel wall a few millimetres thick contributes little. The bed side controls, and it is the hardest to predict, because heat crosses the packing by a combination of gas convection, conduction through pellet contact points and radiation at high temperature. Correlations for it carry uncertainties of tens of percent even within their fitted range.

This is why $U$ in this project is derived from a published duty and area rather than predicted from a correlation, and why the derivation carries a stated band as opposed to a single value.

## The balance

For a plug flow reactor integrated over catalyst mass, with reaction heat released into the flowing gas and optional heat removal through the tube wall:

$$\frac{dT}{dW} = \frac{\sum_j \left(-\Delta H_{r,j}(T)\right) r_j - q_{\text{removed}}}
{\sum_i F_i\,C_{p,i}(T)}$$

The numerator is a net heat rate per unit catalyst mass in $\mathrm{W\,kg^{-1}}$. The denominator is the stream heat capacity rate in $\mathrm{W\,K^{-1}}$. Both temperature dependences are carried; neither $\Delta H_r$ nor $C_p$ is frozen at a reference value.

Reaction enthalpy at temperature comes from the Kirchhoff integration described in the thermodynamics document, so the same formation table and heat capacity correlations serve the energy balance and the equilibrium calculation. There is one enthalpy source in the model.

## Stoichiometry

Two reactions, indexed over the nine species in enum order $(\mathrm{CO_2}, \mathrm{H_2}, \mathrm{CO}, \mathrm{H_2O}, \mathrm{CH_3OH}, \mathrm{CH_4}, \mathrm{N_2}, \mathrm{Ar}, \mathrm{O_2})$:

$$\nu_{\text{MeOH}} = (-1,\,-3,\,0,\,+1,\,+1,\,0,\,0,\,0,\,0)$$

$$\nu_{\text{RWGS}} = (-1,\,-1,\,+1,\,+1,\,0,\,0,\,0,\,0,\,0)$$

The same matrix drives both the mole balance and the heat release, so a stoichiometric error cannot appear in one and not the other.

$$\Delta H_{r,j}(T) = \sum_i \nu_{ij}\,H_i(T)$$

Evaluated at 298.15 K this gives −49.32 and +41.15 $\mathrm{kJ\,mol^{-1}}$, matching the gas-basis values published by Shi and Van-Dal respectively.

## Thermal modes

| Mode | $q_{\text{removed}}$ | Use |
|---|---|---|
| `Adiabatic` | 0 | Van-Dal laboratory reactor |
| `Cooled` | $U a_v (T - T_c)$ | Mucci and Shi boiling water reactors |
| `Isothermal` | $dT/dW = 0$ enforced | diagnostic only |

The reactor's own default is `Adiabatic`, because the validation case it is checked against is Van-Dal's genuinely adiabatic laboratory bed and a default that cooled it would invalidate that check. The plant flowsheet overrides it to `Cooled` at its own layer, since every industrial methanol converter in the cited literature is a jacketed boiling water reactor. Keeping the two separate means the validation gate and the design point can each use the correct boundary condition without one being a special case of the other.

## Specific transfer area

Heat leaves through the tube wall, so the transfer area per unit catalyst mass follows from the tube geometry:

$$a_v = \frac{\text{wall area}}{\text{catalyst mass}}
= \frac{\pi D L}{\rho_{\text{bulk}}\,\tfrac{\pi}{4}D^{2} L}
= \frac{4}{\rho_{\text{bulk}}\,D}$$

Bed length cancels, as it must, since both quantities scale with it. Tube diameter appears inversely: narrower tubes remove heat more effectively per unit of catalyst, which is why industrial methanol reactors use many small tubes instead of few large ones.

$$q_{\text{removed}} = U\,a_v\,(T - T_c)
\qquad \left[\mathrm{W\,kg_{cat}^{-1}}\right]$$

The sign convention is that $q_{\text{removed}}$ is positive when the gas is hotter than the coolant. When the gas is colder, the term is negative and the coolant heats the gas, which is the correct behaviour at a reactor inlet below the coolant temperature.

## Deriving the cooling coefficient

$U$ controls every temperature in a cooled reactor and therefore controls conversion, since methanol synthesis is exothermic and equilibrium-limited. Higher temperature accelerates the kinetics and lowers the attainable conversion. No source paper publishes $U$ for this system.

### What is published

Shi et al. Section 3.1 gives the reactor geometry and duty exactly:

$$A = 2700 \times \pi \times 0.035 \times 7.0 = 2078.2\ \mathrm{m^2}$$

$$Q = 61\ \mathrm{MW} \text{ absorbed to raise high-pressure steam}$$

with the gas entering at 250 $^\circ\mathrm{C}$ and leaving at 275 $^\circ\mathrm{C}$.

The duty is independently consistent with the plant's own production rate. At 2095 $\mathrm{t\,day^{-1}}$ of methanol:

$$\dot n_{\mathrm{MeOH}} = \frac{2095 \times 1000}{32.042 \times 86400} = 756.6\ \mathrm{mol\,s^{-1}}$$

and at −91 $\mathrm{kJ\,mol^{-1}}$ for carbon monoxide hydrogenation, which dominates a syngas loop, the reaction releases 68.9 MW before the endothermic shift contribution. The stated 61 MW is the right magnitude.

### What is not published

The shell-side steam pressure, and therefore the coolant temperature and the driving force. Without it, $U$ is not determined:

$$U = \frac{Q}{A\,\Delta T_m}$$

| $T_{\text{shell}}$ ($^\circ\mathrm{C}$) | $\Delta T_{lm}$ (K) | $U$ ($\mathrm{W\,m^{-2}\,K^{-1}}$) |
|---|---|---|
| 245 | 13.9 | 2104 |
| 234 | 26.6 | 1104 |
| 220 | 41.3 | 711 |
| 200 | 61.7 | 476 |
| 180 | 81.9 | 359 |

A factor of six across plausible steam pressures.

### The value adopted

The coolant temperature is taken as 245 $^\circ\mathrm{C}$, which Mucci states for the same class of reactor and which is already the standing value elsewhere in this model. The required flux is

$$\frac{Q}{A} = \frac{61\times10^{6}}{2078.2} = 29{,}352\ \mathrm{W\,m^{-2}}$$

A boiling water reactor rises steeply to a hot spot in the first fraction of the bed and then sits near its outlet temperature for most of its length, so the effective driving force is closer to the outlet approach of 30 K than to the log-mean of the endpoints. This gives

$$U = \frac{29{,}352}{30} \approx 980\ \mathrm{W\,m^{-2}\,K^{-1}}$$

with an uncertainty band of 700 to 1400 $\mathrm{W\,m^{-2}\,K^{-1}}$ corresponding to effective driving forces of 42 K and 21 K.

This is derived, not sourced, and the flag in the code stays false. The geometry and duty are published; the coolant temperature is imported from a different paper and the driving force is inferred. Derived and sourced are not the same and are not conflated.

### An independent estimate, and why it is not used

A packed-bed wall Nusselt correlation offers a route to $U$ from first principles. Evaluating gas properties at reactor conditions with this model's own viscosity and heat capacity gives $Pr = 0.698$ and, for an assumed loop flow, $Re_p \approx 1.8\times10^{4}$. The Li and Finlayson correlation $Nu_w = 0.17\,Re_p^{0.79}$ then yields $U \approx 3200\ \mathrm{W\,m^{-2}\,K^{-1}}$.

That number is not used, for three reasons. The Reynolds number is a factor of
2.4 outside the correlation's stated validity range of 20 to 7600. The loop mass flow is not published and had to be assumed, and $Nu \propto Re^{0.79}$ makes the result strongly sensitive to it. The thermal conductivity is a modified Eucken estimate instead of measured data.

It is reported here as a negative result instead of as corroboration.

### The previous value

$U$ was 300 $\mathrm{W\,m^{-2}\,K^{-1}}$ before this derivation, an order-of-magnitude placeholder. At that value the Shi geometry could remove only about 19 MW against a published 61 MW. The old figure came from intuition calibrated on lower-pressure systems; at 78 bar the gas-side coefficient is considerably larger.

## Verification

| Check | Result |
|---|---|
| $\Delta H_r$ methanol at 298.15 K | −49.32 against Shi's −49 |
| $\Delta H_r$ shift at 298.15 K | +41.15 against Van-Dal's +41 |
| Stream heat capacity, pure $\mathrm{H_2}$ at 493.15 K | matches $F\,C_p$ directly |
| $a_v$ for a stated bed | matches $4/(\rho_{\text{bulk}}D)$ |
| Heat removal sign | positive above coolant, negative below |
| Isothermal mode | $dT/dW = 0$ exactly |
| Adiabatic mode with exothermic rates | $dT/dW > 0$ |

The consequential check is the adiabatic laboratory case reproducing Van-Dal's per-pass conversion, which is covered in the reactor integration document.

In the code. `energy_balance.hpp` declares the stoichiometry matrix, the thermal modes and the cooling configuration with the full derivation of $U$. `energy_balance.cpp` holds the reaction enthalpy, heat capacity rate, transfer area and the temperature derivative.