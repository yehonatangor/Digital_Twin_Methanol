# Separations

Three separation steps, one modelled from real phase equilibrium and two as performance specifications, with the boundary between them stated.

## Background: separation principles behind these four units

Every separation exploits a difference in some property, and the property chosen determines the equipment.

Volatility difference drives flash and distillation. A flash is a single equilibrium stage and separates only as far as one contact allows. Distillation stacks many stages with counter-current vapour and liquid, so each stage enriches the last, and the separation achievable is limited by relative volatility and the number of stages rather than by equilibrium alone. The governing parameter is

$$\alpha_{ij} = \frac{K_i}{K_j}$$

Values near 1 make separation difficult and require many stages. Methanol and water have $\alpha$ near 4 at atmospheric pressure, which is comfortable, and this is why a methanol column of 50 to 60 stages reaches 99.9 percent purity without difficulty.

Molecular size and solubility difference drives membrane separation. Transport through a dense polymer follows a solution-diffusion mechanism, and the selectivity for one gas over another is the product of a solubility ratio and a diffusivity ratio. Hydrogen is both small and fast-diffusing, so it permeates readily against the larger molecules in a purge stream, which is what makes membrane hydrogen recovery practical.

Nothing at all drives the recycle split. A splitter divides one stream into two of identical composition, changing only the flow. It performs no separation and exists to set a ratio.

### Why a purge is unavoidable in any recycle loop

A recycle loop with no exit accumulates anything that enters but neither reacts nor leaves. Inerts such as argon or nitrogen are the usual culprits, and here carbon monoxide plays the same role because the reverse shift produces it and the synthesis path modelled does not consume it.

The steady-state inert concentration follows from a balance around the loop. If inert enters at rate $\dot n_{\text{in}}$ and the purge takes a fraction $f_p$ of the recycle stream, then at steady state

$$y_{\text{inert}} \approx \frac{\dot n_{\text{in}}}{f_p \, \dot n_{\text{recycle}}}$$

The concentration is inversely proportional to purge fraction. Halving the purge doubles the inert level, which dilutes the reactants and lowers the rate. The purge is therefore a genuine optimisation: too small and the loop chokes, too large and unconverted reactant is thrown away. Recovering value from the purge, which is what the membrane does, shifts that balance.

## Where separations sit

Reactor effluent contains product methanol and water alongside a large unconverted excess of hydrogen and carbon oxides. Per-pass conversion is low, so almost all the value is in recovering the product and returning the rest.

Four steps do this. The effluent is taken to the drum condition, a total load of 157 MW at the design point, most of it sensible but 28 % latent. That load is not all paid for by a utility: a feed-effluent exchanger recovers 96 MW of it into the incoming feed, per Van-Dal Sec. 2.3.1, leaving 60 MW for a trim cooler. Chapter 16 gives the split and the reason it does not move the converged answer. A knockout drum then condenses the product at the reactor's own outlet pressure. A recycle split returns most of the vapour and purges the remainder, and a membrane divides that purge into hydrogen returned to the mixer and tail gas that leaves. A column then purifies the crude methanol into product and a wastewater bottoms.

![Carbon dioxide hydrogenation flowsheet with recycle](figures/01-co2-hydrogenation-flowsheet.svg)

## Knockout drum

Modelled from phase equilibrium. A knockout drum is a flash vessel, and the project already has a validated flash engine, so this module is a named wrapper around `flash::solve` rather than a split-fraction approximation.

That is a deliberate choice. Nothing about the separation is assumed: the real vapour-liquid equilibrium at the specified temperature and pressure determines how much condenses. There is no removal efficiency to source, and none is invented.

Operating temperature and pressure are required arguments instead of defaults. Van-Dal states 35 $^\circ\mathrm{C}$ after water cooling for the synthesis loop separator and Shi states 40 $^\circ\mathrm{C}$, so a default would be picking one plant's condition and applying it everywhere.

The result reports water removal fraction, vapour fraction, whether the mixture was single phase, whether the flash converged, and how many liquid pairs lacked fitted activity parameters. That last field matters here more than anywhere else, because the condensate is exactly the carbon dioxide, methanol and water mixture for which no carbon dioxide interaction parameters exist.

At 78 bar the flash takes the fugacity route, which does have carbon dioxide binary parameters, so the gap is bounded. It would bite at low pressure.

## Recycle and purge

A single split fraction applied to every component:

$$F_i^{\text{recycle}} = f\, F_i, \qquad F_i^{\text{purge}} = (1-f)\, F_i$$

with $f$ a caller-supplied field, 0.99 at the design point.

Both Mucci and Shi state 98 percent, arrived at independently, which is a genuine cross-check instead of one paper citing the other. Shi phrases it as sending 98 percent of the unreacted gas back for recompression and purging the remaining 2 percent. Those plants run on a feed carrying argon; this one does not. Van-Dal, whose flowsheet this one follows, states 1 percent for the same reason discussed below, and that is the value used here.

### Why a purge is needed at all

Recycling everything would be thermodynamically free and is impossible in practice, because the feed carries inerts. Argon at 11 percent in Van-Dal's feed, nitrogen in a reformed gas. Inerts do not react and do not condense, so without a purge they accumulate until they displace the reactants.

The purge fraction sets the steady-state inert concentration. Too small and the loop chokes on argon; too large and unconverted reactant is thrown away. Two percent is the industrial answer for a plant on an argon-bearing feed.

### Why the purge and the feed ratio have to be set together

The two percent above is quoted for a plant whose feed carries 11 percent argon. This flowsheet is fed pure carbon dioxide and electrolytic hydrogen, so there is no argon and the purge is not set by inert accumulation at all. What it removes instead is carbon monoxide from the reverse shift and the hydrogen surplus discussed next, which means the purge cannot be chosen on its own: it depends on the fresh hydrogen to carbon ratio $R$, and the two together decide both the carbon yield and the size of the loop.

The design point takes Van-Dal's own stated 1 percent. That figure is sourced, but it is worth being clear that the model does not itself require it. Carbon monoxide is slightly soluble and leaves dissolved in the crude liquid, so a steady state exists even at zero purge and the loop measurably converges there. The purge is a real requirement in a real plant, for inert ingress and catalyst poisons this model does not represent, and the citation is what fixes its value instead of anything the model discovered.

The stoichiometric ceiling at 88.0 t/h fresh $\mathrm{CO_2}$ is 555.4 $\mathrm{mol\,s^{-1}}$ of methanol, or 17.80 $\mathrm{kg\,s^{-1}}$, one carbon in one carbon out. What the loop actually consumes is not the stoichiometric 3.0. With a carbon-monoxide selectivity $s$ from the reverse water-gas shift, the loop draws

$$R_{\text{consume}} = 3 - 2s$$

moles of hydrogen per mole of carbon, below 3 whenever any $\mathrm{CO}$ is made, measured here at 2.695. Feeding at 3.0 therefore injects a permanent hydrogen surplus, and since hydrogen recycles while carbon is consumed, that surplus accumulates. The reactor inlet drifts to $SN = 17$ against a fresh-feed value of 2, and reaches 94 mol percent hydrogen with the $\mathrm{CO_2}$ partial pressure throttled from 20 to 3 bar.

Accumulation is real, but it is not by itself an infeasibility. In the cooled bed the loop still closes; what the surplus buys is a much larger loop:

| Purge | $R$ | Inlet SN | Conversion | Carbon yield | Recycle | Circulator | Pre-heat |
|---|---|---|---|---|---|---|---|
| 5 % | 2.40 | 1.6 | 80.0 % | 77.5 % | 5.0 × fresh | 1.3 MW | 54 MW |
| 5 % | 2.70 | 4.9 | 88.8 % | 86.3 % | 6.2 × fresh | 2.0 MW | 69 MW |
| 5 % | 2.85 | 9.0 | 92.5 % | 89.9 % | 7.9 × fresh | 3.7 MW | 90 MW |
| 5 % | 3.00 | 17.1 | 95.1 % | 92.0 % | 11.7 × fresh | 10.6 MW | 135 MW |
| 1 % | 2.95 | 8.6 | 97.8 % | 97.2 % | 8.3 × fresh | 4.8 MW | 96 MW |
| 1 % | 3.00 | 18.3 | 98.8 % | 98.0 % | 13.8 × fresh | 18.9 MW | 160 MW |

Carbon yield rises monotonically with $R$ across the whole grid, so trimming the ratio does not buy yield. It buys a compact loop: at $R = 2.40$ the circulator is a factor of ten smaller and the pre-heat a factor of three, on a recycle stream less than half the size.

The full envelope is worth plotting, because the trade is not the simple monotone curve the table above suggests.

![Operating envelope: carbon yield against loop compression duty](figures/03-operating-envelope.svg)

Two things in that figure are not obvious from the numbers. First, along $R = 2.85$ and $R = 2.95$ a tighter purge raises carbon yield and lowers circulator duty at the same time, because a higher conversion per pass shrinks the recycle faster than the tighter purge grows it. Only at $R = 3.00$ does the accumulating hydrogen surplus dominate, and there the curve turns sharply upward. Second, that makes $R = 3.00$ a dominated choice: $R = 2.95$ at a 1 percent purge reaches 97.2 percent carbon yield for a 4.8 MW circulator, against
94.8 percent for 13.2 MW at $R = 3.00$ and a 3 percent purge, at the same catalyst activity floor of 0.60.

The feasible region is also not convex. At $R = 2.70$ the loop converges at 3 percent purge and again at 1 percent, but fails at 2 percent, and the failure is a genuine integration failure inside the bed instead of an iteration limit. Any optimiser run over this space has to be told that, and it is the reason the sweep is reported as a grid of measured points instead of as a fitted curve.

Every point in that figure is a converged solve, not an interpolation. The sweep that produces them is `tools/operating_envelope.cpp` and its output is kept beside the figure as `figures/03-operating-envelope.csv`, so the figure can be regenerated instead of trusted. It solves the loop 25 times plus up to four aged solves per converged node, so it takes minutes.

Trimming also caps what is attainable. Feeding $R$ below 3.0 limits the carbon yield to $R/3$, here 80.0 percent, because the hydrogen runs out first. The five percent row reaches 77.5 percent, or 96.1 percent of that cap, so that case is close to the limit its own feed allows and more catalyst cannot move it. The untrimmed rows have no such cap, which is why they keep climbing.

The trade also decides where the bed becomes the constraint. At $R = 2.40$ the loop fails below two percent purge: the recycle grows until the Ergun drop across the tube bundle consumes the available loop pressure and the integrator reports a right-hand-side failure. At $R = 3.00$ the loop survives to one percent, because the higher conversion per pass keeps the required recycle from growing as fast.

The same constraint reappears as the catalyst ages, and it does not bind equally on the two branches. Lower activity means less conversion per pass, which means more recycle for the same duty, which means a larger Ergun drop. The design point at $R = 2.95$ runs an 8.3 times recycle when the catalyst is fresh, so it reaches that limit at an activity of about 0.45. The stoichiometric $R = 3.00$ at the same purge runs 13.8 times and gives up at 0.8. The compact branch starts at 5.0 times and is still converging at 0.2. Carbon yield, compression duty and tolerance to ageing are three faces of the same trade, and a design chosen on the first alone will be surprised by the third.

This picture is specific to the cooled bed. In adiabatic operation the same sweep at $R = 3.0$ fails at ten percent purge and below, and trimming the ratio to 2.40 was what closed the loop at all. Cooling changes the answer because it holds the bed near the temperature where the exothermic equilibrium is favourable, so the dilute hydrogen-rich inlet still reacts. The thermal boundary condition and the feed policy are therefore not independent choices, and reporting one without the other gives a misleading picture of either.

### Reproducing the reference plant's conversion

Van-Dal reports about 93 percent overall $\mathrm{CO_2}$ conversion on a 44,500 kg catalyst charge. This flowsheet reproduces that on the published charge, with no scaling of the bed:

| Purge | $R$ | Overall conversion | Carbon yield |
|---|---|---|---|
| 8 % | 3.00 | 92.6 % | 88.3 % |
| 7 % | 3.00 | 93.4 % | 89.5 % |
| 5 % | 3.00 | 95.1 % | 92.0 % |
| 3 % | 3.00 | 96.9 % | 94.8 % |
| 1 % | 2.95 | 97.8 % | 97.2 % |
| 1 % | 3.00 | 98.8 % | 98.0 % |

The match at 7 percent should be read for exactly what it is. The purge fraction is not published, so this is a one-parameter fit to a single reported number, not a blind prediction. What it does establish is that the published charge and the published conversion are consistent with each other under this model, at a purge in the range a real plant would run. Solving an adiabatic bed cannot close the loop below a ten percent purge at $R = 3.0$. With the jacketed reactor, the reference plant's numbers are reachable and the loop closes seamlessly.

The per-pass conversion is 50 percent at the canonical point, against the 33 percent Van-Dal reports. Those two are not comparable: Van-Dal's figure is a laboratory measurement at 50 bar and 220 $^\circ\mathrm{C}$ on 34.8 g of catalyst, while this is a plant-scale bundle at 78 bar fed a hydrogen-rich recycle. The comparison that matters is the overall conversion, which is a plant-boundary quantity in both cases.

The design point therefore sits on the published charge at Van-Dal's own stated 1 percent purge and $R = 2.95$, giving 97.8 percent conversion and a 97.2 percent carbon yield. Both the bed geometry and the feed ratio remain caller-supplied fields, so the trade-off above can be swept instead of argued about.

### Degassing the crude before the column

Sending 72 bar condensate straight into a column near atmospheric pressure skips a step a real plant performs. The flowsheet therefore lets the crude down to 25 bar, the middle of the usual 20 to 30 bar band, and flashes it through the same Peng-Robinson and NRTL machinery the knockout drum uses.

What that achieves is not what a first guess suggests:

| Dissolved species in the column feed | At 71.6 bar | After the 25 bar let-down |
|---|---|---|
| Hydrogen | 1.364 $\mathrm{mol\,s^{-1}}$ | 0.448 $\mathrm{mol\,s^{-1}}$ |
| Carbon dioxide | 3.713 $\mathrm{mol\,s^{-1}}$ | 3.572 $\mathrm{mol\,s^{-1}}$ |

Hydrogen is barely soluble and comes out readily, roughly two thirds of it. Carbon dioxide does not, and this is correct instead of a modelling failure: methanol is a good physical solvent for carbon dioxide, which is the entire basis of the Rectisol process. Four percent of it leaves against sixty-seven percent of the hydrogen. A let-down flash cannot strip it, and a model that claimed otherwise would be wrong.

Methanol lost with the vent is 0.0074 $\mathrm{mol\,s^{-1}}$ against 539.2 $\mathrm{mol\,s^{-1}}$ in the crude, about one part in $10^{5}$ of the product, so the degassing step costs essentially nothing. Whatever permanent gas survives the flash is still accounted for at the column, which reports it as a vent instead of dissolving it into the wastewater.

### Loop diagnostics worth reporting

A recycle loop can be converged, mass-balanced and still sitting at an operating point no engineer would choose. Three numbers reveal it and the result structure now carries all three.

Loop stoichiometric number. Lim Eq. (4) evaluated at the converged reactor inlet instead of on the fresh feed. Hydrogen recycles while carbon is consumed, so the loop drifts hydrogen-rich. At the design point the fresh feed is at $SN = 1.95$ and the converged reactor inlet sits at $SN = 8.6$, more than four times the value the feed was blended to. That is a real property of feeding near stoichiometry, not an error, and it is what the 8.3 times recycle and the
4.8 MW circulator are paying for. At $R = 3.00$ the same diagnostic reads 18.3. On the compact branch the same diagnostic reads 1.6. Reporting it is how the two branches are told apart at a glance.

Peak bed temperature. The maximum along the integrated profile, not the outlet. In cooled operation the peak is 524.1 K at about a third of the tube and the outlet is 521.5 K, so the interior maximum is real but shallow. That sits just inside the lower edge of Fichtl's fitted 523 to 553 K band, so the deactivation correlation is used in range. It is a near thing: at $R = 3.00$ the more dilute inlet drops the peak to 522.7 K and the validity layer flags the excursion. In adiabatic operation on the same feed the bed runs tens of kelvin hotter, which is the difference a tube-wall design and a deactivation model both have to see.

Chemical energy in the tail gas. The membrane retentate carries 4.8 MW of lower heating value, against 364.9 MW in the methanol product, so 1.3 percent of the chemical energy leaving the loop leaves as fuel. At a 30 percent purge those figures were 203.1 MW against 201.6 MW, an even split. That is the carbon-yield result of the previous section restated in energy terms, and it is the number that decides whether this flowsheet is a power-to-methanol plant or a plant with a large fuel by-product.

## Membrane hydrogen recovery

A recovery fraction applied to hydrogen only:

$$F_{\mathrm{H_2}}^{\text{permeate}} = 0.90\,F_{\mathrm{H_2}}^{\text{feed}}$$

with everything else reporting to the retentate. Mucci states 90 percent.

This is a performance specification, not a transport model. A rigorous membrane needs permeance per species, area, and the pressure ratio across it, none of which Mucci publishes. What is published is the recovery, and that is what is implemented.

The consequence worth stating: the model cannot answer how recovery responds to a change in area or pressure ratio, because that relationship is not in it. It answers what happens at the stated performance.

### Permeate pressure, and why it dominates the compression bill

Separation across a polymeric membrane is driven by a partial pressure difference, so the permeate has to leave well below the retentate. Typical delivery is 5 to 25 bar against a retentate near feed pressure. Mucci publishes no value, so the membrane module leaves both outlets at feed pressure and the flowsheet exposes the real pressure as a configurable input instead of inventing one.

The choice is not a detail. Returning the permeate to a 78 bar mixer means lifting it from wherever the membrane delivers it, and the work scales with the pressure ratio:

| Permeate delivered at | Booster duty | Total loop compression |
|---|---|---|
| 71.6 bar, the module's own contract | 0.04 MW | 4.88 MW |
| 15 bar, mid-band for a real membrane | 0.99 MW | 5.82 MW |

A factor of 25 on the booster and 1.2 on the total. Taking the permeate at feed pressure is therefore not a conservative simplification, it is a large understatement of the recovery cost, and `permeate_pressure_assumed_at_feed` reports which case a given result used.

The booster is small here because a 1 percent purge is a small stream. The penalty for guessing the permeate pressure wrong therefore scales with the purge instead of with the plant, and at this design point it is a rounding error against the circulator. It was not always: at a 30 percent purge the same comparison ran 2.75 MW against 48.2 MW.

The alternative a real plant would use is routing the permeate into an intermediate stage of the hydrogen feed compressor instead of boosting it separately, which recovers part of that work. The model charges the separate booster, which is the conservative direction.

Purge gas is hydrogen-rich, so recovering most of it before the remainder is burned as fuel is worth real money. This unit is why the purge is not simply a loss.

The unit resolves into two streams and both are followed. The permeate returns to the mixer, so it becomes part of the loop's tear stream and is included in the convergence test. The retentate leaves the boundary as tail gas, carrying the inerts and the carbon oxides the loop needs to reject. At the design point the membrane returns 13.0 $\mathrm{kg\,s^{-1}}$ of hydrogen.

Because the permeate is a return path, it also has to appear in any mass balance around the loop. Checking that fresh feed plus recycle equals the reactor inlet fails by exactly the recovered hydrogen; the correct closure is fresh plus recycle plus permeate.

## Distillation

Two specifications, both from Shi Section 2.5:

$$\text{methanol recovery} = 0.995, \qquad \text{product purity} = 0.999\ \text{wt}$$

The distillate takes 99.5 percent of the feed methanol, plus enough water to land on the purity target:

$$m_{\mathrm{H_2O}}^{\text{dist}} = m_{\mathrm{CH_3OH}}^{\text{dist}} \frac{1 - x_{\text{purity}}}{x_{\text{purity}}}$$

capped at the water actually present. Everything else goes to bottoms. If the cap binds, the achieved purity is reported instead of the target, so the caller sees when the specification was not met.

### Why lumped instead of stage-by-stage

Both papers describe the column in some detail. Shi gives 22 stages, feed at stage 12, a 65.0 MW reboiler and 64.1 MW condenser. Van-Dal gives 44 rectifying and 13 stripping stages at a reflux ratio of 1.2.

Neither publishes what a rigorous MESH model needs: tray-by-tray equilibrium data, activity coefficients fitted to their exact column, or the operating lines. And their duties are absolute values tied to their plant scale as opposed to per-unit-throughput correlations.

Building a stage model would mean inventing the missing tray-level numbers. This module implements what is published and dimensionless, and is therefore scale-independent: a recovery fraction and a purity target.

This is the fidelity boundary of the model. The column is where a reader should expect the least resolution, and saying so is more useful than implying otherwise. If a stage model is ever needed, the activity coefficient infrastructure exists and the missing piece is data, not machinery.

The heat exchange module's duty functions can estimate a reboiler load from this project's own enthalpy data if required, instead of borrowing Shi's plant-scale absolute figures.

## Summary of what is modelled how

| Unit | Basis | What is assumed |
|---|---|---|
| Effluent cooler | real phase equilibrium plus Clausius-Clapeyron latent heat | nothing; T and P are inputs |
| Knockout drum | real vapour-liquid equilibrium | nothing; P is the reactor outlet |
| Recycle split | fraction, 0.70 in the loop | Mucci and Shi state 0.98; the bed limits it |
| Membrane | recovery, 0.90 | Mucci; no transport model, no pressure drop |
| Distillation | recovery and purity, 0.995 and 0.999 | Shi; no stage model |

The first is physics. The other three are published performance figures. The distinction is worth keeping visible, because a reader deciding whether to trust a separation result needs to know which kind it is.

## Verification

| Check | Result |
|---|---|
| Knockout material balance | vapour plus liquid equals feed, to $10^{-12}$ |
| Knockout at 78 bar | selects the fugacity route |
| Water removal fraction | consistent with the flash liquid |
| Recycle plus purge | equals feed exactly, component by component |
| Recycle fraction guards | outside $[0,1]$ rejected |
| Membrane | permeate plus retentate equals feed; non-hydrogen untouched |
| Distillation purity | achieved value equals target unless water-limited |
| Distillation with no methanol | returns cleanly instead of dividing by zero |

In the code. `front_end/knockout_drum.hpp` and `.cpp` wrap the flash. `recycle.hpp`, `membrane.hpp` and `distillation.hpp` with their implementations hold the three specification models, each with the citation for its figure and a statement of what it does not model.