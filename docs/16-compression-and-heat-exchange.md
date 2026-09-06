# Compression and Heat Exchange

Isentropic compression, single-stream enthalpy balances, a two-stream exchanger, and a cooler that accounts for condensation.

## Background: compression work and the isentropic reference

Compressing a gas requires work, and how much depends on the path. Three idealised paths bracket the possibilities.

Isothermal compression holds temperature constant by removing heat as fast as it is generated. It requires the least work and is unattainable in a real machine, because heat transfer is never that fast.

Adiabatic and reversible, meaning isentropic, exchanges no heat with the surroundings. Gas leaves hot, and the work is

$$W_{\text{is}} = \frac{k}{k-1}RT_{\text{in}}\left[\beta^{(k-1)/k} - 1\right]$$

with $\beta$ the pressure ratio and $k = C_p/C_v$. This is the standard reference case for a real compressor, since a machine running at high throughput has little time to exchange heat.

Polytropic compression follows $PV^n$ = constant with $n$ fitted to the real path, and is the convention manufacturers usually quote for multistage machines.

Real work exceeds the isentropic ideal, and the ratio defines the isentropic efficiency:

$$\eta_{\text{is}} = \frac{W_{\text{is}}}{W_{\text{actual}}}$$

Centrifugal machines fall in the range 0.7 to 0.85. A separate mechanical efficiency accounts for bearing, seal and gearbox losses between the driver and the gas, and it is a property of the machine rather than the fluid.

### Why the heat capacity ratio appears

The exponent $(k-1)/k$ carries the thermodynamics of the path. For a monatomic gas $k = 1.67$, for a diatomic gas 1.4, and for larger molecules it falls toward
1.1 as vibrational modes absorb energy. A higher $k$ means a steeper temperature rise for a given pressure ratio, and therefore more work.

Hydrogen has $k \approx 1.41$ and a very low molar mass, which is the difficulty with hydrogen compression: the work per unit mass is high because a kilogram of hydrogen is a great many moles.

### Why multistage with intercooling

The work expression grows faster than linearly with $\beta$, and discharge temperature rises with it. Splitting the duty into $N$ stages of equal ratio $\beta^{1/N}$ and cooling back to inlet temperature between them reduces total work, because each stage begins with a colder and denser gas. It also keeps discharge temperatures within material limits. Equal ratio per stage is the optimum for identical stages, which is the arrangement used for the carbon dioxide train in this project.

## Compression

Hydrogen leaves the electrolyser at 20 to 40 bar and the synthesis loop runs at 78 bar, so compression is unavoidable.

Mucci Section 3.3 gives the shaft power for an isentropic compression with efficiency losses:

$$P_{\text{comp}} = \frac{\dot m\, c_p\, T_{\text{in}}}{\eta_{\text{is}}\,\eta_{\text{mech}}}
\left(\beta^{\frac{k-1}{k}} - 1\right)$$

with pressure ratio $\beta = P_{\text{out}}/P_{\text{in}}$ and heat capacity ratio $k = c_p/c_v$.

### The heat capacity ratio is computed

$k$ is not stored as 1.41 for hydrogen. It is derived at the inlet temperature from the same heat capacity correlation everything else uses, through the ideal gas Mayer relation:

$$c_v = c_p - R, \qquad k = \frac{c_p}{c_p - R}$$

on a molar basis, where $k$ is basis-independent. Mass-basis $c_p$ for the power expression follows by dividing by molar mass.

The reason is consistency rather than accuracy. A tabulated $k$ would be a second source of truth for hydrogen's heat capacity, free to drift from the DIPPR correlation in `species.cpp`. Computing it means one number governs both.

The code rejects a degenerate case where $c_p \le R$, which cannot occur for a real gas but would produce a negative or infinite $k$ if a heat capacity correlation were ever mis-entered.

### Efficiencies

$\eta_{\text{is}} = 0.8$ is stated directly by Mucci.

$\eta_{\text{mech}} = 0.877$ is derived, not printed. It was back-solved from Mucci's Table A.2 validation data, which compares their model against a commercial simulator across several pressure ratios at 1900 $\mathrm{kg\,h^{-1}}$ and 40 bar inlet. The value is the mean of the ratios implied by those rows.

`eta_mechanical_sourced` is false. The derivation is defensible and reproducible, but it is a derivation and the flag says so.

### Turndown

A compressor has a minimum stable flow, below which it surges. The result carries an advisory flag when per-unit flow falls under half of rated capacity, which is Mucci's stated single-unit operating range.

This is advisory instead of an error, because the model has no surge correlation and cannot claim the machine has actually surged. It marks an operating point that a real installation would need a recycle valve or a second machine to reach, which matters for dispatch, where low-price hours push the electrolyser and therefore the compressor to part load.

## Single-stream heating and cooling

Two inverse operations on the same energy balance.

Given a target temperature, find the duty. Direct evaluation:

$$Q = \sum_i F_i \left[H_i(T_{\text{target}}) - H_i(T_{\text{in}})\right]$$

Given a duty, find the temperature. Newton iteration on the same expression, using the stream heat capacity as the exact derivative:

$$T_{n+1} = T_n - \frac{\sum_i F_i H_i(T_n) - H_{\text{target}}}{\sum_i F_i c_{p,i}(T_n)}$$

The derivative is exact instead of a finite difference, because $dH/dT = c_p$ by definition and both come from the same correlation. Newton converges in three or four iterations from any reasonable start.

Both use `thermo::enthalpy`, so a temperature change computed here and a reaction enthalpy computed in the reactor rest on the same formation data.

## Two-stream exchanger

### Background: LMTD against effectiveness-NTU

Two methods size a heat exchanger, and the choice depends on what is known.

The log mean temperature difference method applies when all four terminal temperatures are known. Duty follows from $Q = UA\,\Delta T_{\text{lm}}$, where the log mean

$$\Delta T_{\text{lm}} = \frac{\Delta T_1 - \Delta T_2}{\ln(\Delta T_1/\Delta T_2)}$$

is the correct average because the local driving force decays exponentially along the exchanger. This is the design case: given a required duty, find the area.

The effectiveness-NTU method applies when the area is known but the outlet temperatures are not, which is the rating case and the one this module solves. It works with three dimensionless groups:

$$\varepsilon = \frac{Q}{Q_{\max}}, \qquad
\mathrm{NTU} = \frac{UA}{C_{\min}}, \qquad
C_r = \frac{C_{\min}}{C_{\max}}$$

The maximum possible duty is set by the stream with the smaller heat capacity rate, since that stream reaches the other's inlet temperature first: $Q_{\max} = C_{\min}(T_{h,\text{in}} - T_{c,\text{in}})$. Effectiveness is the fraction of that maximum actually achieved.

NTU is a dimensionless size. Effectiveness rises steeply with NTU up to about 3 and then flattens, which is the economic argument against oversizing: doubling area beyond that point buys very little extra recovery.

Counterflow outperforms parallel flow at the same NTU, because parallel flow drives both streams toward a common intermediate temperature and cannot exceed 50 percent effectiveness with balanced streams, while counterflow can approach 100 percent. Real shell and tube units fall between the two and carry a correction factor $F$ on the log mean.

### The implementation

Recuperative exchange between a hot and a cold process stream, by the effectiveness-NTU method:

$$\mathrm{NTU} = \frac{UA}{C_{\min}}, \qquad C_r = \frac{C_{\min}}{C_{\max}}$$

$$\varepsilon_{\text{counter}} = \frac{1 - \exp\left[-\mathrm{NTU}(1-C_r)\right]}{1 - C_r\exp\left[-\mathrm{NTU}(1-C_r)\right]}$$

$$Q = \varepsilon\, C_{\min} \left(T_{h,\text{in}} - T_{c,\text{in}}\right)$$

### No default UA anywhere

`UA_W_per_K` is a required argument with no default.

None of the source papers publish a UA for any exchanger in this flowsheet. The reactor cooling coefficient had a derivable route through Shi's stated duty and geometry, documented separately, but the process exchangers have nothing equivalent. Supplying a plausible number would create a free parameter that looks like data, and an optimiser exploring heat integration would find and lean on it.

The caller must provide UA from a real design or a value they own and state.

### A stated approximation

Effectiveness-NTU assumes constant heat capacity rates across the exchanger. Each stream's $C = \dot m c_p$ is evaluated at its own inlet temperature and held fixed. That is standard for a sizing estimate and approximate for streams with strongly temperature-dependent $c_p$ or a wide temperature span. For a single stream where variable heat capacity matters, the exact functions above integrate the real $c_p(T)$ with no such assumption.

## Where the two-stream exchanger is actually used: feed-effluent integration

The two-stream model is not decorative. It carries the largest heat integration in the flowsheet, and leaving it out was a real deviation from the reference paper.

Van-Dal Sec. 2.3.1 describes the arrangement directly: the gases leaving the reactor are divided, and the first stream, 60 percent of the total, heats the fresh feed in HX4. The synthesis loop therefore has a feed-effluent exchanger, not a standalone heater and a standalone cooler working against each other.

### Why it does not move the answer

The reactor inlet temperature is a specified design condition, 210 $^\circ\mathrm{C}$. The exchanger cannot change it. What it changes is where the heat comes from:

$$Q_{\text{preheat}} = Q_{\text{FEHE}} + Q_{\text{trim heater}}$$

and correspondingly how much cooling a utility still has to do after the effluent has already given up heat to the feed. Converged compositions, conversions, pressures and iteration count are identical with the exchanger on or off. The test suite asserts that identity to $10^{-12}$, which is what makes the duty comparison below meaningful instead of a side effect of a shifted operating point.

### The duties at the design point

At 88.0 t/h fresh $\mathrm{CO_2}$, a 1 percent purge and $R = 2.95$, with $UA = 5\times10^{6}$ $\mathrm{W\,K^{-1}}$:

| Quantity | Standalone | With FEHE |
|---|---|---|
| Feed pre-heat required | 96.4 MW | 96.4 MW |
| Recovered from the effluent | 0 MW | 96.4 MW |
| Trim heater, external | 96.4 MW | 0 MW |
| Trim cooler, external | 156.6 MW | 60.2 MW |
| Exchanger effectiveness | n/a | 0.903 |

The exchanger covers the whole pre-heat, so no trim heater is needed and the cooling utility falls by 62 percent. The full effluent load between the reactor and the drum is unchanged at 156.6 MW; the exchanger moves 96.4 MW of it onto the feed instead of onto cooling water.

Both duties scale with the recycle, not with the fresh feed. On the compact branch, a 5 percent purge at $R = 2.40$, the same exchanger handles a 54.1 MW pre-heat and a 97.5 MW effluent load on a recycle stream 5.0 times the fresh feed instead of 8.3 times. At the stoichiometric $R = 3.00$ and a 1 percent purge the recycle grows to 13.8 times and the pre-heat to 160 MW. The heat integration is therefore not a fixed saving to be quoted once; it is a duty that follows the feed policy.

### An accounting rule worth stating

The full cooling load and the trim cooling load are each computed directly, not one from the other by subtraction. Effectiveness-NTU works on a mean heat capacity while the condensing cooler integrates the real enthalpy path including condensation, so the two routes disagree by about 1 MW. Subtracting one from the other would attribute that disagreement to the trim cooler.

### What is left cold

The recycle returns from the knockout drum as cold vapour, and the circulator rejects its heat of compression, so the recycle re-enters the mixer near the drum temperature instead of at reactor temperature. This matters: handing the recycle back hot collapses the pre-heat duty by an order of magnitude and hides the largest heating load in the flowsheet behind a modelling convenience.

## Condensing cooler

The single-stream functions are ideal gas and single phase. That is right for gas preheat and wrong for the cooling duties this flowsheet spends most of its energy on.

Taking a reactor effluent containing methanol and water from 500 K to 313 K crosses the dew point. An ideal gas balance understates the duty by the entire latent heat and returns a vapour outlet below its own dew point.

`cool_with_condensation` handles this by reusing machinery that already exists instead of adding new physics:

1. `flash::solve` at the target temperature and pressure gives the real phase split.
2. The sensible term is the ideal gas enthalpy change for the whole stream.
3. The latent term is the enthalpy of vaporisation of whatever condensed.

$$Q = \underbrace{\sum_i F_i \left[H_i(T_2) - H_i(T_1)\right]}_{\text{sensible}}
- \underbrace{\sum_i F_i^{L} \Delta H_{\mathrm{vap},i}(T_2)}_{\text{latent}}$$

### Enthalpy of vaporisation from the vapour pressure curve

$\Delta H_{\mathrm{vap}}$ is not a separate table. It comes from the Clausius-Clapeyron slope of the same DIPPR-101 correlation the flash uses to decide what condenses:

$$\Delta H_{\mathrm{vap}}(T) = R T^{2} \frac{d \ln P^{\text{sat}}}{dT}$$

evaluated by central difference on the smooth correlation. Deriving it from the vapour pressure curve keeps the two consistent by construction. A separate heat-of-vaporisation table would be a second source of truth able to drift out of step, so that a species could be predicted to condense while carrying a latent heat fitted to different data.

Species with no vapour pressure data return zero, which is correct: they do not condense in this model and carry no latent term.

The result returns both outlet phases, so a caller feeding a knockout drum does not re-flash, and reports the latent share of the duty along with the flash convergence and unparameterised-pair count.

## Verification

| Check | Result |
|---|---|
| Heat capacity ratio for hydrogen | derived from `thermo::cp`, matches the Mayer relation |
| Compression power | reproduces Mucci Section 3.3 at their stated case |
| Degenerate $c_v \le 0$ | rejected |
| Heat then cool round trip | returns the original temperature |
| Duty and temperature inverses | mutually consistent |
| Effectiveness-NTU limits | $\varepsilon \to 1$ as NTU grows, $\to 0$ as NTU vanishes |
| Condensing cooler with no condensate | reduces exactly to the single-phase duty |
| Latent fraction | reported, non-zero only when the flash produces liquid |

In the code. `front_end/compressor.hpp` and `.cpp` hold the compression model and the derived mechanical efficiency with its provenance note. `heat_exchanger.hpp` and `.cpp` hold the single-stream balances, the effectiveness-NTU exchanger with its no-default UA contract, and the condensing cooler with the Clausius-Clapeyron derivation.