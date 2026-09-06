# Reactor Integration

The coupled ordinary differential system, the integrator, and two numerical properties the test suite enforces that comparison against literature does not cover.

## Background: the plug flow model and its coordinate

Two idealised reactor models bracket real behaviour. A continuous stirred tank is mixed, so composition is uniform and equals the outlet. A plug flow reactor has no axial mixing, so composition varies continuously along the length and every fluid element has the same residence time. A packed tube with a length to diameter ratio of 200, as here, is close to plug flow.

The plug flow mole balance is a differential equation rather than an algebraic one. Over a differential slice:

$$\frac{dF_i}{dV} = \sum_j \nu_{ij}\,r_j$$

For a catalytic reaction the natural independent variable is not volume but catalyst mass, because published rate laws are reported per kilogram of catalyst rather than per cubic metre of reactor. Using $W$ removes the bed density from the balance and puts it in one place, the conversion between $W$ and axial position, which is where it belongs:

$$W = \rho_{\text{bulk}} A_c z$$

The choice matters for portability. A rate law expressed per unit catalyst mass transfers between beds of different void fraction without modification, while one expressed per unit volume does not.

### Why the system is stiff and what that costs

The eleven equations do not evolve on one timescale. Near the inlet the reaction rate is at its highest and composition changes quickly; further down the bed the approach to equilibrium slows everything. Temperature and pressure change on different scales again. A system whose characteristic rates differ by orders of magnitude is called stiff, and stiffness is what forces small steps: the integrator must resolve the fastest mode even where the slow ones dominate.

Runge-Kutta methods are explicit, meaning each step is computed directly from the previous one. They are simple, self-starting and cheap per step, but their stability region is bounded, so a stiff problem forces the step size down until the fastest mode is resolved regardless of the accuracy needed. Implicit methods such as BDF, used in commercial simulators through solvers like DASSL, have unbounded stability regions and take large steps on stiff problems, at the cost of solving a nonlinear system at every step.

The choice here is explicit RK4 with a step size set by the catalyst inventory. For the beds in this project the resulting step counts are in the thousands, which is affordable, and the method has no hidden state, no solver tolerance to tune, and no dependence on a linear algebra library. Reproducibility was weighted above speed, for the reason given below.

## The system

Eleven coupled equations integrated over catalyst mass $W$ from zero to the bed inventory: nine species molar flows, temperature and pressure.

$$\frac{dF_i}{dW} = \sum_j \nu_{ij}\,r_j\left(\mathbf{y}, T, P\right)
\qquad i = 1 \dots 9$$

$$\frac{dT}{dW} = \frac{\sum_j \left(-\Delta H_{r,j}\right) r_j - q_{\text{removed}}}
{\sum_i F_i C_{p,i}}$$

$$\frac{dP}{dW} = \frac{1}{\rho_{\text{bulk}} A_c}\left(\frac{dP}{dz}\right)_{\text{Ergun}}$$

The coupling is complete. Rates depend on composition, temperature and pressure. Temperature depends on rates and on the heat capacity of the current composition. Pressure depends on density, which depends on composition, temperature and pressure through the equation of state.

## Integrator

Classical fourth-order Runge-Kutta, fixed step:

$$\mathbf{y}_{n+1} = \mathbf{y}_n + \frac{h}{6}\left(\mathbf{k}_1 + 2\mathbf{k}_2 + 2\mathbf{k}_3 + \mathbf{k}_4\right)$$

Four right-hand side evaluations per step, each requiring two rate expressions, nine enthalpies, nine heat capacities and one cubic equation of state solve.

Fixed step instead of adaptive, because the result must be exactly reproducible for a given configuration. A dataset generated for machine learning has to be regenerable, and an adaptive controller makes the step sequence depend on floating-point details that differ between compilers.

## Step sizing

A step count is not a safe way to specify the grid, because accuracy depends on step size $h = W_{\text{end}}/n$.

The Van-Dal laboratory bed holds 0.0348 kg of catalyst per tube. The plant-scale bed holds 7.17 kg, a factor of 206 more. A step count tuned on the laboratory bed becomes a 206 times coarser step on the plant bed while looking identical in the configuration.

This is not hypothetical. At a fixed 500 steps the plant case reported
1.884 $\mathrm{kg\,s^{-1}}$ of methanol against a grid-converged 1.711 $\mathrm{kg\,s^{-1}}$, a 10 % overstatement, while the laboratory validation case passed cleanly throughout because it is converged at 50 steps.

The grid is therefore specified by a target step size, with the count demoted to a lower bound:

$$n_{\text{eff}} = \max\!\left(n_{\text{steps}},\; \left\lceil \frac{W_{\text{end}}}{h_{\max}} \right\rceil\right)$$

with $h_{\max} = 2\times10^{-3}\ \mathrm{kg}$ for methanol synthesis. This can only refine a grid, never coarsen one, so a caller that pinned a step count still receives at least that many.

Both bounds are needed and they protect against different things. The size ceiling protects large beds. The count floor protects small ones, where stiffness comes from reaction rate and thermal coupling instead of from bed mass: the laboratory bed needs only 18 steps by the size rule but is not converged there.

### Convergence

Plant-scale bed, adiabatic, stoichiometric feed:

| Steps | $X_{\mathrm{CO_2}}$ | Methanol ($\mathrm{kg\,s^{-1}}$) | $T_{\text{out}}$ (K) |
|---|---|---|---|
| 500 | 0.19198 | 1.88423 | 503.62 |
| 2,000 | 0.18123 | 1.71945 | 499.98 |
| 3,587 (default) | 0.18078 | 1.71278 | 499.83 |
| 8,000 | 0.18068 | 1.71131 | 499.80 |
| 50,000 | 0.18068 | 1.71120 | 499.80 |

The default sits within 0.09 % of a fourfold-finer grid. A regression test asserts that refining by four changes methanol yield by less than 1 %.

## The non-negativity clamp

A species approaching depletion can be driven slightly negative by the truncation error of a step. A negative molar flow poisons the mole fractions and therefore the rate expression, so the state is clamped at zero.

The clamp is correct for rounding-level excursions and dangerous for anything larger, because it only ever adds material. Clamping a genuine overshoot fabricates moles of the depleted species and breaks the element balance in one direction.

The clamp therefore measures itself:

$$\text{clamp}_{\text{rel}} = \max_{\text{steps},\,i}
\frac{\max(0, -F_i)}{\max_j F_j^{\text{inlet}}}$$

and the integration aborts if this exceeds a tolerance of $10^{-9}$, which is far above floating-point noise and far below anything physically real.

This matters because the failure is otherwise silent. In the tri-reforming reactor, where oxygen is the limiting reagent for a fast combustion, a coarse grid produced a 4.3 % oxygen surplus while carbon and hydrogen still closed to $10^{-14}$ and the total mass check stayed green.

## Atom conservation

Element balances are asserted instead of assumed:

$$C = F_{\mathrm{CO_2}} + F_{\mathrm{CO}} + F_{\mathrm{CH_3OH}} + F_{\mathrm{CH_4}}$$

$$H = 2F_{\mathrm{H_2}} + 2F_{\mathrm{H_2O}} + 4F_{\mathrm{CH_3OH}} + 4F_{\mathrm{CH_4}}$$

$$O = 2F_{\mathrm{CO_2}} + F_{\mathrm{CO}} + F_{\mathrm{H_2O}} + F_{\mathrm{CH_3OH}} + 2F_{\mathrm{O_2}}$$

Each must be conserved to $10^{-10}$ relative between inlet and outlet. At the default grid all three close to $10^{-16}$.

An element balance is strictly stronger than a total mass balance. Total mass can remain closed while one species is fabricated and another destroyed in compensating amounts, and the oxygen case above is exactly that situation.

## Guard rails

The integration stops and reports instead of continuing when the state leaves a configured envelope:

| Guard | Default | Meaning |
|---|---|---|
| $T_{\min}$ | 200 K | quench or a sign error in the energy balance |
| $T_{\max}$ | 900 K | thermal runaway |
| $P_{\min}$ | $10^{4}$ Pa | pressure drop has consumed the driving pressure |
| clamp | $10^{-9}$ | fabricated moles |

Each returns `ok = false` with a message naming the cause, instead of returning a plausible-looking state.

## Real gas density

Density enters through the pressure drop term:

$$\rho = \frac{P\,\bar{M}}{Z\,R\,T}$$

with $Z$ from the Peng-Robinson equation. At 78 bar the departure from ideality is significant enough to affect the mass flux and therefore the pressure drop.

The equation of state solve is the most expensive operation in the loop, roughly 45 % of the total, and can be disabled for a first estimate. A failed solve falls back to $Z = 1$ instead of propagating an exception through the integrator.

## Two-stage operation

Mucci's configuration places a condenser between two cooled reactor stages. Removing methanol and water from the intermediate stream shifts the equilibrium of the second stage and raises overall conversion beyond what a single bed of the same total mass achieves.

The interstage separator calls the same flash used everywhere else, on the stage-one outlet at its own pressure. The condensate is reported separately and the vapour continues to stage two.

Two quantities are not published by Mucci and are labelled as modelling choices: the condenser temperature, borrowed from Shi's final separator at 40 $^\circ\mathrm{C}$, and the stage-two inlet temperature, taken as the coolant temperature on the reasoning that a cooled reactor preheats its feed against the shell side.

No isolated validation case exists for this configuration in any of the source papers, which the module states directly. What is checked is that material balances close: inlet mass equals stage-two outlet plus interstage condensate to within $10^{-4}$ relative.

## Verification

| Check | Result |
|---|---|
| Van-Dal per-pass $\mathrm{CO_2}$ conversion | 33.84 % against 33 % |
| Mass conservation, laboratory case | to $10^{-16}$ |
| Atom balance, carbon hydrogen oxygen | to $10^{-16}$ |
| Grid independence at fourfold refinement | under 0.1 % |
| Adiabatic outlet above inlet | 553.12 K from 493.15 K |
| Guard rail trip on a low $T_{\max}$ | reports and stops |
| Coarse grid contract | refuses instead of succeeding |
| Two-stage material balance, outlet plus condensate | to $10^{-4}$ relative |
| Two-stage pressure bookkeeping, 1 bar per stage | 75 bar in, 73 bar out |
| Interstage removal beats one stage alone | $2.05$ against $1.53\times10^{-4}$ mol/s |

The guard-rail and coarse-grid rows are contract tests: they assert that the model fails under conditions where it should, which is a property no comparison against published values would catch.

The three two-stage rows are bookkeeping, not validation, and the distinction matters here. Mucci publishes the operating data for the configuration but no stream table and no tube dimensions, so there is no isolated case to reproduce. What is checkable is that the stated 1 bar per stage appears at the outlet, that mass survives the interstage flash once the condensate is counted, and that removing product between the stages produces more methanol than one stage alone. `two_stage_reactor.hpp` says the same thing in its own header, and `tests/test_two_stage_reactor.cpp` also pins `interstage_T_sourced` to false, because the 40 C interstage temperature is borrowed from a final separator in a different paper.

In the code. `reactor_core.hpp` declares the state, configuration and result types, including the grid and clamp diagnostics. `reactor_core.cpp` holds the right-hand side, the Runge-Kutta loop, the step-size selection and the measured clamp. `two_stage_reactor.cpp` holds the interstage sequence.