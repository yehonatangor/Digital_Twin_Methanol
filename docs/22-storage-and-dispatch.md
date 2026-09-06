# Storage and Dispatch

A hydrogen buffer between a variable electrolyser and a steady synthesis loop, and a price rule that decides when to run.

## Background: why a power-to-methanol plant needs a buffer

The two halves of this plant want opposite things. Electrolysis is the flexible half, because electricity price varies by the hour and a stack can be turned down or off, so its economics improve if it runs mostly when power is cheap. Methanol synthesis is the inflexible half. A catalyst bed at 250 $^\circ\mathrm{C}$ in a recycle loop has a thermal time constant of hours, and cycling it wastes energy and shortens catalyst life, so it wants a constant feed.

The mismatch is resolved with storage. Hydrogen produced during cheap hours is banked and drawn down during expensive ones, letting the electrolyser follow price while the synthesis loop sees a constant supply. The vessel size then becomes a genuine design variable: too small and the plant cannot ride through an expensive period, too large and the capital is wasted.

Compressed gas storage is the relevant technology at this scale. The stored mass depends on pressure through the equation of state, and the useful working capacity lies between two pressures. The floor is set by the downstream plant, because hydrogen below its inlet pressure cannot be delivered without recompression. The ceiling is set by the vessel's mechanical design.

## Source

Mucci et al., *Journal of Energy Storage* 72 (2023) 108614, Sec. 2.4 and 3.4 for the storage model and Appendix A.2 and A.3 for the density fit and the vessel cost. This is the same paper the compressor and electrolyser modules are grounded in.

## The mass balance

Sec. 3.4, in the paper's own notation:

$$M_{\mathrm{H_2}}(t) = M_{\mathrm{H_2}}(t-\Delta t)
+ \dot m_{\mathrm{H_2,prod}}(t)\,\Delta t
- \dot m_{\mathrm{H_2,MeOH}}(t)\,\Delta t$$

A plain discrete accumulator. Production comes from the electrolyser and consumption from the synthesis plant. The implementation does exactly this and nothing more.

## Pressure from stored mass

Sec. 3.4 again:

$$M_{\mathrm{H_2}}(t) = V \left[\rho(p(t)) - \rho(75\ \mathrm{bar})\right]
\approx V \times 0.073\ \frac{\mathrm{kg}}{\mathrm{m^3\,bar}}
\times \left(p(t) - 75\ \mathrm{bar}\right)$$

This is a linear fit of hydrogen density against pressure, taken from Fig. A.2, generated from an Aspen Peng-Robinson database at 25 $^\circ\mathrm{C}$, which is Mucci's own stated isothermal storage temperature. Inverting,

$$p(t) = p_{\min} + \frac{M_{\mathrm{H_2}}(t)}{V \times 0.073}$$

Hydrogen is a long way from ideal at these pressures, which is why a linear fit rather than the ideal gas law is used, and why the fit is referenced to a specific temperature.

The reference pressure of 75 bar is not arbitrary. Mucci sets the storage minimum to the synthesis pressure, because delivery must meet the downstream inlet pressure. The stored mass is therefore measured relative to that floor: $M_{\mathrm{H_2}} = 0$ means the vessel is at 75 bar and empty in the useful sense, not physically empty.

The vessel volume $V$ has no default. Mucci treats it as a continuous optimisation variable, so the module leaves it as a caller input rather than inventing a size, in the same way the heat exchanger leaves $UA$ and the costing module leaves the material factor to the caller.

## Two constraints, both reported

The floor is physical. Stored mass cannot go negative, because the vessel cannot supply hydrogen below the pressure the downstream plant needs. When a requested consumption exceeds what is available, the withdrawal is truncated to the mass actually there, the state lands at exactly zero, and a floor violation is flagged. That flag is the point: an unmet-demand step means the schedule that produced it does not work, and a dispatch loop must be able to detect this instead of running a negative inventory.

The ceiling is inferred, and flagged as such. Mucci states a floor and never states an operating ceiling. The module originally had none, and the consequence was instructive: the demo run charged the vessel to 621 bar over twenty four hours and reported no violations at all. That is beyond any realistic Type I vessel and far outside the range the linear density fit was drawn over, so both the mechanics and the thermodynamics were being extrapolated.

The ceiling adopted is 160 bar, taken from Mucci's own vessel cost basis, which prices 160 bar stationary tanks. That is the only vessel design pressure the paper commits to anywhere. Using it as the operating limit makes the mass balance consistent with the capital cost the same module computes, since charging past 160 bar would mean storing hydrogen in a vessel nobody paid for.

The inference is recorded instead of presented as a quotation. Mucci gives 160 bar as a cost reference, not as a stated operating limit, and the `p_max_from_cost_basis` flag says so. Setting the ceiling at or below the floor disables it and recovers the original unbounded behaviour.

When the ceiling is reached, surplus hydrogen is reported as vented instead of accumulated. A real plant would curtail the electrolyser or vent, and either way the schedule is infeasible as written. Floor and ceiling violations are counted separately, because an undersized vessel and an over-eager production rule are different faults with different fixes.

The working capacity follows directly:

$$M_{\text{capacity}} = V \times 0.073 \times (p_{\max} - p_{\min})$$

For a 1000 $\mathrm{m^{3}}$ vessel between 75 and 160 bar that is 6205 kg.

## Vessel capital cost

Appendix A.3, verbatim:

$$\text{Cost} \left[\frac{\$}{\mathrm{m^3}}\right] =
500\ \frac{\$}{\mathrm{kg}} \times 0.073\ \frac{\mathrm{kg}}{\mathrm{m^3\,bar}}
\times (160 - 1)\ \mathrm{bar} \times \mathrm{UF}$$

The 500 dollars per kilogram is a stated target cost for 160 bar stationary gaseous hydrogen tanks in 2020. The middle term is the available hydrogen mass per unit volume at that reference design, reusing the same density slope as the operating relation instead of deriving a second one. The update factor of 1.35 is Mucci's own year-to-year adjustment. The result is 7834.7 dollars per cubic metre against Mucci's stated "around 7800", agreeing within their rounding.

This figure is pinned to a specific reference design: 160 bar, Type I, 2020 basis. It is not re-derived for an arbitrary design pressure, and the module does not extrapolate it to conditions the paper does not cover. A caller wanting a different basis passes different arguments.

## The dispatch rule

What this implements is a myopic single-pass price threshold. At each timestep, if the spot price is at or below a caller-set threshold the electrolyser runs at its configured power; otherwise it idles and the synthesis plant's hydrogen demand is drawn from storage.

This is not Mucci's optimiser. Their flexible-operation results come from solving a full nonlinear design and scheduling optimisation over the entire price horizon at once, with the formulation and solver code in supplementary material outside this project's library. Reproducing that is out of scope. A price threshold is a real and commonly used heuristic in its own right, but a myopic rule and a horizon optimiser give different answers, and results from this module are not compared against, and should not be read as reproducing, any of Mucci's reported cost or flexibility figures.

The electrolyser physics is not re-derived here. When dispatched on, the module calls the existing electrolyser model at the configured power and pressure, following the same composition pattern used elsewhere: the module adds a rule, not new physics. Storage coupling is likewise a direct call to the mass balance above, with electrolyser output as production and a caller-set constant as demand.

Per step, the electricity cost is

$$C_{\text{step}} = P_{\text{total}} \times \frac{\Delta t}{3600} \times \text{price}$$

with power in MW, the timestep in seconds and price in dollars per MWh. Note that $P_{\text{total}}$ is the electrolyser's total draw including auxiliaries, not the stack power alone.

The run result aggregates total cost, hydrogen produced and consumed, steps run, both violation counts, the vented total and the final state.

## Price data

The price series is a plain caller-supplied vector in dollars per MWh, which converts against the operating cost convention as 0.06 USD/kWh equals 60 USD/MWh. No price data is fabricated in this module and no network call is made anywhere in this project. The tests exercise it with clearly labelled synthetic series, pending a real historical series from a public source.

## Worked behaviour

From the test suite, over a synthetic 24 hour series with twelve cheap hours and twelve expensive ones, against a threshold between them:

| Quantity | Value |
|---|---|
| Timesteps | 24 |
| Steps on | 12, exactly the cheap block |
| Total electricity cost | 252 USD |
| Cost on an idle step | 0 |

Raising the threshold above the expensive block runs all 24 hours and costs more, which is the rule behaving as stated. Requesting a draw with no production trips the floor on every step and runs nothing, which is the constraint behaving as stated.

## Modules

| File | Contents |
|---|---|
| `dispatch/storage` | Mass balance, pressure relation, floor and ceiling, vessel capital cost |
| `dispatch/dispatch_loop` | The price threshold rule over a supplied series |

## Verification

| Check | Result |
|---|---|
| Sourced defaults: 75 bar, 0.073 $\mathrm{kg/m^{3}/bar}$, 298 K | exact |
| Pressure and mass are exact inverses | round trip to $10^{-9}$ |
| Working capacity between 75 and 160 bar | 6205 kg for 1000 $\mathrm{m^{3}}$ |
| Vessel cost per cubic metre | 7834.7 against a stated "around 7800" |
| Floor: withdrawal truncated, state lands exactly empty | asserted |
| Pressure never falls below the floor | asserted |
| Ceiling: surplus vented, state pinned at capacity and 160 bar | asserted |
| Dispatch fires on price and only on price | 12 of 24 steps |
| Idle steps cost nothing | exact zero |
| Empty series and non-positive timestep rejected | asserted |

The two violation tests are the ones that matter. They assert that a constraint is detected and reported instead of that a number is reproduced, and the ceiling test exists because its absence produced a 621 bar vessel and a clean report.