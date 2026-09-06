# Catalyst Deactivation

Copper catalyst loses activity over a campaign. Two presets describe that loss, one measured in a laboratory and one calibrated against an industrial curve, and the difference between them is roughly a factor of twelve.

## Background: how catalysts lose activity

A catalyst is not consumed by the reaction it accelerates, but it does degrade. Four mechanisms account for nearly all industrial deactivation, and they differ in reversibility and in how they respond to operating changes.

Sintering is the growth of small metal crystallites into larger ones, driven by the reduction in surface energy. Surface area falls, and since activity scales with exposed metal area, so does the rate. It is thermally activated and effectively irreversible: a sintered catalyst cannot be restored by any treatment short of remanufacture. Copper is particularly susceptible because its Hüttig temperature, where surface atoms become mobile, is around 130 $^\circ\mathrm{C}$, well below the operating temperature of a methanol reactor.

Poisoning is the strong chemisorption of an impurity onto active sites. Sulphur is the classic poison for copper and nickel, and even parts per billion in the feed will accumulate over a campaign. Plants guard against it with upstream sulphur removal rather than by modelling it.

Fouling is the physical deposition of material, usually carbon. It is the dominant problem in reforming and is why the steam to carbon ratio is held above a minimum. It is often reversible by controlled burn-off.

Phase transformation covers oxidation, reduction, or reaction of the active phase with the support into an inactive compound.

For a copper, zinc oxide and alumina catalyst under methanol synthesis conditions, sintering dominates. Water accelerates it, which matters here because carbon dioxide hydrogenation produces a mole of water per mole of methanol, so a $\mathrm{CO_2}$-fed plant ages its catalyst faster than a $\mathrm{CO}$-fed one at the same temperature.

### Representing deactivation in a model

Rather than model crystallite growth from first principles, engineering practice introduces a dimensionless activity $a$ that scales the rate:

$$r(t) = a(t) \cdot r_{\text{fresh}}$$

with $a = 1$ for fresh catalyst. The decay is then written as an empirical power law in the activity itself:

$$\frac{da}{dt} = -K_d(T)\,a^{m}$$

The order $m$ controls the shape of the decline, and the shape carries real information:

- $m = 1$ gives exponential decay, a constant fractional loss per unit time.
- $m = 2$ gives a hyperbolic decline, fast initially then slowing.
- Higher orders give a sharper initial drop followed by a long, flat tail.

High orders describe sintering well because the process is self-limiting. Early on there are many small crystallites with high surface energy and they coarsen quickly; once the distribution has shifted to larger particles the driving force is much reduced and further growth is slow. An industrial charge that loses 30 percent of its activity in the first year and another 20 percent over the next four is exhibiting exactly this behaviour, which is why the industrial preset below uses fifth order.

### Compensating for lost activity

A plant does not shut down when activity falls. It raises reactor temperature to restore the rate, since the Arrhenius term recovers what activity has lost. A typical campaign starts near the low end of the catalyst's window and ends 20 to 30 K higher.

The compensation is self-limiting in two ways, and both set the end of the campaign. Raising temperature accelerates the sintering that caused the problem, so the ramp steepens over time. And for an exothermic equilibrium reaction, higher temperature lowers the attainable conversion, so past some point the extra rate no longer buys extra product. The charge is replaced when the temperature needed exceeds the mechanical or equilibrium limit.

This is the behaviour the temperature-ramp calculation later in this document tests, and reproducing the right magnitude of ramp is what confirmed the mechanism in the model was correct even when the rate was not.

## The decay law

Both presets use the same empirical power law, differing in order and rate:

$$\frac{da}{dt} = -K_d(T)\, a^{m}$$

where $a$ is activity relative to fresh catalyst, so $a = 1$ at start of run. The temperature dependence is Arrhenius:

$$K_d(T) = A \exp\!\left(-\frac{E_d}{RT}\right)$$

For $m \neq 1$ this integrates in closed form at constant temperature:

$$a(t) = \left[a_0^{\,1-m} + (m-1)K_d t\right]^{\frac{1}{1-m}}$$

The closed form is what the RK4 integrator is validated against. RK4 exists for the case the closed form cannot handle, which is a temperature history that varies over the campaign, and that is the realistic case: plants raise temperature to compensate for lost activity, which accelerates the loss.

Time is kept in hours and $K_d$ in reciprocal hours throughout this module, matching the source bookkeeping, instead of converted to the project's SI second basis. A caller coupling this to a reactor run converts once at the boundary.

## The laboratory preset

Fichtl et al. (2015) aged three $\mathrm{Cu/ZnO/Al_2O_3}$ samples at three temperatures and fitted Eq. (1) at each condition independently. Their Table 4 gives the rate constants and orders.

All six values were verified digit for digit against the source . The Arrhenius pairs below are derived from them, not printed in the paper.

| Catalyst | $E_d$ ($\mathrm{kJ\,mol^{-1}}$) | $A$ ($\mathrm{h^{-1}}$) | $K_d$ at 523 K | $K_d$ at 553 K |
|---|---|---|---|---|
| CZA1 | 47.422 | 78.978 | $1.4500\times10^{-3}$ | $2.6200\times10^{-3}$ |
| CZA2 | 39.009 | 10.544 | $1.3399\times10^{-3}$ | $2.1798\times10^{-3}$ |
| CZA3 | 50.893 | 59.902 | $4.9499\times10^{-4}$ | $9.3399\times10^{-4}$ |

Each fit reproduces both of its Table 4 points to six significant figures, which it must, being a two-point fit through two points. The value of stating it is that a transcription error in either constant would break the reproduction.

### Why only two of the three temperatures

Fichtl's fitted order is not constant. At 523 K and 553 K it is 3. At 493 K it is 4.

That is a real feature of their fits, not a typo. It means no single $(K_d, m)$ pair covers their whole 493 to 553 K range, so the Arrhenius fits above are built from the two third-order points only, and are defensible near that band.

The 493 K point is exposed separately as a standalone constant, $K_d =
4.29\times10^{-3}\ \mathrm{h^{-1}}$ with $m = 4$, instead of folded into a smooth $K_d(T)$ curve. Folding it in would turn a genuine order change into apparent continuous behaviour and produce a number that looks right and is not.

### Water

Fichtl reports that water accelerates deactivation strongly, with an activity loss above 90 percent against roughly 60 percent dry over a comparable aging period. That is one co-feed experiment against one dry baseline, and the co-feed ran at a different space velocity, 0.72 $\mathrm{h^{-1}}$ against 0.51 $\mathrm{h^{-1}}$.

No fitted water pressure exponent is published, and a two-point ratio across different space velocities would not isolate the effect cleanly. So there is no $K_d(T, p_{\mathrm{H_2O}})$ relation in this module.

Instead there is an opt-in multiplier on $K_d$ with a default of 1.0 and a `water_multiplier_sourced` flag that is false. The default means dry gas kinetics, which is what Table 4 was measured under. This is the same no-invented- default discipline the heat exchanger applies to UA, and for the same reason: carbon dioxide hydrogenation makes water stoichiometrically, so a fabricated water exponent would sit directly in the path of any optimiser tuning the carbon monoxide to carbon dioxide ratio.

### A caveat on the feed

Fichtl's aging gas is 13.5 percent carbon monoxide against 3.5 percent carbon dioxide, roughly 3.9 to 1. This project's Van-Dal-based feed is 4 percent to 3 percent, roughly 1.3 to 1.

Sun, Metcalfe and Sahibzada (1999) found deactivation severity tracks the carbon monoxide fraction specifically: under carbon dioxide and hydrogen at differential conversion it is negligible, under carbon monoxide and hydrogen it is severe and correlates with loss of copper surface area.

So Fichtl's rates should be read as an upper bound for this project's more carbon dioxide rich gas, not a prediction for it. No paper in the library publishes a carbon-monoxide-fraction-dependent $K_d$, so the caveat is stated instead of absorbed into the numbers.

## Kinetic limits

Inverting Fichtl's closed form for CZA1 at 245 $^\circ\mathrm{C}$, which is this project's synthesis temperature:

$$t_{a=0.5} = \frac{0.5^{-2} - 1}{2 K_d}, \qquad K_d(518.15\ \mathrm{K}) = 1.3093\times10^{-3}\ \mathrm{h^{-1}}$$

$$t_{a=0.5} = \frac{3}{2 \times 1.3093\times10^{-3}} = 1146\ \mathrm{h} = 0.131\ \mathrm{years}$$

Six weeks to half activity, and 0.204 at one year. Industrial charge life is measured in years, typically three to five with a gradual temperature ramp.

This is not an error in Fichtl and not a transcription problem. It is an extrapolation from lab powder under accelerated conditions to industrial pellets over years, and re-reading the source does not repair it.

### Isolating what was wrong

instead of choose between accepting the fast rate or flagging it as a caveat, the model tests whether the *mechanism* was wrong or only the *rate*.

The test: if activity falls to 0.5, how much extra temperature restores the original methanol production rate through the LHHW kinetics?

The answer, computed from the project's own rate expression instead of estimated, was +26.9 K. A plant would ramp from 245 $^\circ\mathrm{C}$ to about 272 $^\circ\mathrm{C}$ over the campaign.

That is precisely what industrial methanol plants do, and the magnitude matches practice. So the deactivation mechanism, its coupling to the kinetics, and the compensating response are all correct. The defect was isolated to a single scalar, the rate constant, which made the problem sourceable instead of structural.

## The industrial preset

Kordabadi and Jahanmiri (2007) Eq. (4) gives a deactivation model they state was found by Hanken to be suitable for industrial applications:

$$\frac{da}{dt} = -K_d \exp\!\left[-\frac{E_d}{R}\left(\frac{1}{T} - \frac{1}{T_R}\right)\right] a^{5}$$

Two things change from the laboratory preset, and only two.

### The order becomes fifth

Fifth order produces the characteristic industrial shape: a fast decline over the first months, then a long slow tail. Kordabadi describe their own Fig. 5 in exactly those terms, activity declining rapidly in the first few months and slowly thereafter.

This is not a curve-fitting convenience. The shape difference is what makes a multi-year campaign possible at all. Third order with a slow rate would still decay too uniformly.

### The rate is calibrated, not printed

Kordabadi do not publish $K_d$. They cite Hanken's 1995 NTNU MSc thesis for it, which is not in the library.

What they do publish is an operating anchor. Their Fig. 5 spans 1400 days, and their optimisation sweeps activity levels of 0.9, 0.8, 0.7, 0.6, 0.5 and 0.4 as representing the activity trend over catalyst lifetime.

So $A$ is calibrated to place $a = 0.40$ at 1400 days and 245 $^\circ\mathrm{C}$. Inverting the fifth-order closed form:

$$K_d = \frac{a^{1-m} - 1}{(m-1)\,t} = \frac{0.40^{-4} - 1}{4 \times 33600\ \mathrm{h}} = 2.8320\times10^{-4}\ \mathrm{h^{-1}}$$

$$A = K_d \exp\!\left(\frac{E_d}{RT}\right) = 17.083\ \mathrm{h^{-1}}$$

This is calibration against a published industrial curve, not an invented scaling factor. The distinction is that the target value came from the source and only the constant that reaches it was solved for.

### The resulting trajectory

| Time | $a$, industrial | $a$, Fichtl lab |
|---|---|---|
| 3 months | 0.732 | 0.385 |
| 6 months | 0.640 | |
| 1 year | 0.550 | 0.204 |
| 2 years | 0.468 | |
| 3 years | 0.425 | |
| 1400 days | 0.400 (anchor) | |

Time to reach $a = 0.5$ at 245 $^\circ\mathrm{C}$: 1.51 years industrial against 0.131 years laboratory, a factor of 11.5.

The trajectory reproduces Kordabadi's stated shape with no tuning beyond the single anchor. Half the total decline happens in the first year and the remaining decline stretches over the following two and a half.

### What is still missing

$E_d$, which governs how strongly decay accelerates with temperature, is in Hanken and not in Kordabadi. Fichtl's CZA1 value of 47.422 $\mathrm{kJ\,mol^{-1}}$ is used as a stated interim, on the reasoning that both describe the same sintering mechanism and only the rate differs between lab powder and industrial pellets.

This matters specifically for the temperature-ramp case. A campaign simulation that raises temperature to hold production constant is sensitive to $E_d$, because the ramp itself accelerates the decay it is compensating for. A steady- temperature result is not.

`kIndustrialEdSourced` is false and `sourced` on the config is false. The form is sourced to Kordabadi Eq. (4). The rate is calibrated against their published curve. Calibrated is not sourced, and the two are not conflated anywhere in the code or in this document.

To close it: L. Hanken, MSc thesis, Norwegian University of Science and Technology, 1995, cited as reference [19] in Kordabadi, where the surname is misspelled Honken.

## Coupling to the kinetics

The rate module is a pure function validated to $10^{-4}$ against Van-Dal's worked example, and the reactor calls it directly with no notion of activity. Threading an activity argument through those signatures would modify validated code.

The coupling is therefore one level up, at the call site:

```cpp
double r_fresh = lhhw::r_CH3OH(P_CO2, P_H2, P_H2O, P_CH3OH, T);
double r_aged = activity_decay::scale_rate(r_fresh, current_activity);
```

`scale_rate` is a plain multiplication, named so that every place the coupling happens is visible by grep instead of hidden inside an arithmetic expression.

## What deactivation does to conversion, and what it does not

A reasonable expectation is that halving the catalyst activity halves the conversion. In this reactor it does not, and the reason is worth stating because it changes which quantity is the honest one to report.

Van-Dal's laboratory bed is equilibrium limited instead of rate limited. It holds enough catalyst that the outlet composition reaches the exothermic equilibrium ceiling well before the end of the bed. Removing activity shortens the approach to that ceiling but does not move the ceiling itself, so the outlet is unchanged until the bed becomes short enough, in effective catalyst terms, to stop reaching it at all.

Measured on the laboratory case:

| Activity | $\mathrm{CO_2}$ conversion | Methanol, mol/s | Outlet $T$, K |
|---|---|---|---|
| 1.00 | 0.3384 | $7.094 \times 10^{-5}$ | 553.1 |
| 0.80 | 0.3384 | $7.094 \times 10^{-5}$ | 553.1 |
| 0.50 | 0.3384 | $7.094 \times 10^{-5}$ | 553.1 |
| 0.20 | 0.3401 | $7.008 \times 10^{-5}$ | 552.1 |
| 0.05 | 0.3031 | $3.321 \times 10^{-5}$ | 514.9 |

Two things in that table are easy to misread as bugs and are not.

Conversion is flat from $a = 1.0$ down to $a = 0.5$. That is the equilibrium ceiling, not a coupling that has failed to take effect. The methanol column does move over the same range, in the seventh significant figure, which is what confirms the activity multiplier is genuinely reaching the rate law.

Conversion at $a = 0.20$ is slightly *higher* than at $a = 1.0$. Methanol synthesis is exothermic, so a slower bed releases heat more gradually, runs cooler, and a cooler bed has a more favourable equilibrium constant. The outlet temperature column shows the mechanism directly: 552.1 K against
553.1 K. The gain is small and real, and it disappears by $a = 0.05$ once the bed can no longer reach equilibrium at all.

The practical consequence is that conversion is the wrong metric for tracking deactivation in this bed and production is the right one. Methanol production falls monotonically with activity across the whole range; conversion does not. The test suite asserts the monotonic quantity and asserts the equilibrium plateau, instead of tolerating the flat region as noise.

This also explains why the plant-scale recycle loop behaves differently. There the bed is sized against a much larger throughput, the loop is closer to rate limited, and activity moves production directly: 16.88 kg/s at $a = 1.0$ falls to 16.15 kg/s at $a = 0.6$. Whether deactivation shows up as lost conversion or as nothing at all depends on which side of the equilibrium ceiling the bed is operating, which is a property of the bed and the throughput, not of the decay law.

The loop also has an activity floor, and the decay law is not what sets it. Lower activity means less conversion per pass, which means the recycle must grow to hold the same duty, which raises the Ergun drop until the loop pressure is consumed. At the design point that floor is near $a = 0.60$. On the compact branch, which starts from a recycle five times the fresh feed instead of twelve, it is near $a = 0.20$. Catalyst life as the flowsheet experiences it is therefore set jointly by the decay law and by the loop's hydraulic headroom, and the second is the binding one here.

## Which preset to use

Use the industrial preset for anything spanning a plant campaign: economics, dispatch, replacement scheduling, and any machine learning trained on multi-year behaviour.

Use the laboratory preset for what it measured, which is accelerated aging over hundreds of hours, and as a conservative bound.

Both are exposed. Neither is a default that a caller can pick up without choosing.

## Verification

| Check | Result |
|---|---|
| RK4 against the closed form | agrees to integrator tolerance |
| Fichtl fits reproduce Table 4 | both points, all three catalysts, 6+ figures |
| 493 K point | separate constant, $m = 4$, not interpolated |
| Water multiplier | defaults to 1.0, flag false |
| Non-isothermal history | RK4 path exercised against a ramp |
| Industrial order | 5, per Kordabadi Eq. (4) |
| Calibration anchor | $a = 0.400$ at 1400 days, 245 $^\circ\mathrm{C}$ |
| Industrial against laboratory | time to $a = 0.5$ longer by more than 10× |
| Guards | $a_0 \notin (0,1]$, $t < 0$, $K_d < 0$, $T \le 0$ all rejected |
| Activity 1.0 against the fresh reactor | reproduces `integrate_reactor` bit for bit |
| Equilibrium plateau | conversion flat from $a = 1.0$ to $a = 0.5$, asserted |
| Production monotonicity | methanol non-increasing in activity across the range |

In the code. `degradation/activity_decay.hpp` carries the full provenance including the three stated non-modelled items, the preset declarations and the calibration anchor constants. `activity_decay.cpp` holds the closed form, the RK4 integrator and the preset constructions, with the anchor inversion written out in the industrial preset instead of a hard-coded pre-exponential. 