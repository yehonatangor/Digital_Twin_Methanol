# Flash Calculations

Isothermal vapour-liquid equilibrium at fixed temperature and pressure, the regime split between two thermodynamic formulations, and the guards that keep the iteration from converging on nothing.

## Background: what a flash is and why it is posed this way

A flash is the calculation behind every separator in a flowsheet. A stream at some condition is brought to a new temperature and pressure, and the question is how much of it becomes vapour, how much becomes liquid, and what each phase contains. The name comes from the original application, where a pressurised liquid crossing a valve partly vaporises.

The specification matters. A flash needs exactly two intensive variables fixed plus the feed composition, following the phase rule. Fixing $T$ and $P$ gives an isothermal flash, which is the case here and the simplest, because the equilibrium ratios can be evaluated without reference to an energy balance. Fixing $P$ and enthalpy gives an adiabatic flash, which wraps an energy balance around the isothermal calculation and is what a real valve or a reactor effluent cooler performs. Fixing $P$ and vapour fraction at zero or one gives the bubble and dew point calculations used later for column sizing.

Three conditions define equilibrium between phases: equal temperature, equal pressure, and equal chemical potential for every component. The first two are imposed by the specification. The third is what the calculation solves, and it is expressed through fugacity as described in the equation of state document.

The whole problem is compressed into the equilibrium ratio

$$K_i = \frac{y_i}{x_i}$$

which measures a component's tendency to favour the vapour. $K_i > 1$ means component $i$ concentrates in the vapour, $K_i < 1$ in the liquid. For an ideal system Raoult's law gives $K_i = P_i^{\text{sat}}/P$ directly, so $K$ is set by volatility alone. For a real system $K_i$ depends on composition as well, which is what makes the calculation iterative: the phase compositions determine the $K$ values, and the $K$ values determine the phase compositions.

The resulting fixed-point structure is solved here by successive substitution, sometimes called direct iteration. An initial $K$ estimate gives a phase split, the split gives updated $K$ values from the thermodynamic model, and the cycle repeats to convergence. The Wilson correlation supplies the starting estimate.

The same fixed-point problem appears one level up, around the recycle loop, where the tear stream is iterated rather than the $K$ values. There the convergence rate matters enough to justify acceleration, which is covered in the reactor and flowsheet documents.

## The problem

Given a feed of composition $z_i$ at temperature $T$ and pressure $P$, find the vapour fraction $\psi$ and the two phase compositions $x_i$ and $y_i$ that satisfy equilibrium and material balance.

Material balance per component, with $F = 1$ basis:

$$z_i = \psi\,y_i + (1-\psi)\,x_i$$

Equilibrium, expressed through the distribution ratio:

$$K_i = \frac{y_i}{x_i}$$

Eliminating $y_i$ gives the phase compositions in terms of $\psi$ and $K_i$:

$$x_i = \frac{z_i}{1 + \psi(K_i - 1)}, \qquad y_i = K_i x_i$$

## Rachford-Rice

Requiring the mole fractions to sum to one in both phases gives a single scalar equation in $\psi$:

$$f(\psi) = \sum_i \frac{z_i(K_i - 1)}{1 + \psi(K_i - 1)} = 0$$

This form is preferred over $\sum x_i = 1$ or $\sum y_i = 1$ separately because it is monotonically decreasing in $\psi$ over the physical interval, which makes the root unique and bracketing reliable.

$$\frac{df}{d\psi} = -\sum_i \frac{z_i(K_i-1)^2}{\left[1 + \psi(K_i-1)\right]^2} < 0$$

### Phase detection before iteration

Evaluating $f$ at the interval endpoints determines whether the mixture is two phase at all, without any iteration:

$$f(0) = \sum_i z_i(K_i - 1), \qquad f(1) = \sum_i \frac{z_i(K_i-1)}{K_i}$$

If $f(0) \le 0$ the feed is a subcooled liquid and $\psi = 0$. If $f(1) \ge 0$ it is a superheated vapour and $\psi = 1$. Only when $f(0) > 0 > f(1)$ does a root exist in $(0,1)$.

Bisection is used instead of Newton. The derivative is available in closed form, but bisection cannot leave the bracket, and robustness matters more than speed here because the flash sits inside a recycle loop that calls it hundreds of times.

## Two thermodynamic formulations

The equilibrium ratio $K_i$ requires a model for both phases, and the
appropriate choice depends on pressure.

$$\varphi\text{-}\varphi:\quad K_i = \frac{\phi_i^L}{\phi_i^V} \qquad\qquad \gamma\text{-}\varphi:\quad K_i = \frac{\gamma_i P_i^{\text{sat}}}{P}$$

The $\varphi$-$\varphi$ form uses the same equation of state for both phases. It is consistent at high pressure and near the critical region, and it handles supercritical components, which matters here because hydrogen at 78 bar and 500 K is far above its critical point.

Where each one is actually selected in this flowsheet:

| Unit | Pressure | Route |
|---|---|---|
| Synthesis loop | 78 bar | $\varphi$-$\varphi$, Peng-Robinson |
| Knockout drum | 71.6 bar | $\varphi$-$\varphi$, Peng-Robinson |
| Membrane and purge | 71.6 bar | $\varphi$-$\varphi$, Peng-Robinson |
| Distillation and column sizing | 1.2 bar | $\gamma$-$\varphi$, NRTL with DIPPR-101 |

This is the split a process engineer would specify by hand: a cubic equation for the high-pressure gas loop, an activity model for the polar methanol and water liquid in the column. The regime test applies it automatically from pressure, so a unit cannot silently be evaluated on the wrong basis.

The $\gamma$-$\varphi$ form uses an activity model for the liquid and treats the vapour as ideal. It is more accurate for strongly non-ideal liquids at low pressure, where a cubic equation of state describes a polar liquid poorly, but it has no meaning for a component above its critical temperature because $P_i^{\text{sat}}$ does not exist.

The switch is at 10 bar:

```cpp
Regime decideRegime(double P) {
  return (P > 10e5) ? Regime::PhiPhi : Regime::GammaPhi;
}
```

This is a modelling choice, not a sourced value. Ten bar is roughly where the ideal-vapour assumption behind $\gamma$-$\varphi$ stops being defensible for this mixture. Both knockout drums and the interstage condenser run at 78 bar and therefore take the $\varphi$-$\varphi$ path.

## Successive substitution

The outer loop alternates between solving for the phase split at fixed $K$ and
recomputing $K$ from the resulting compositions:

1. Initialise $K_i$ from the Wilson correlation
2. Solve Rachford-Rice for $\psi$
3. Compute $x_i$ and $y_i$
4. Recompute $K_i$ from the phase model at those compositions
5. Repeat until $K$ stops changing

The Wilson correlation supplies the starting estimate:

$$K_i^{(0)} = \frac{P_{c,i}}{P}\exp\!\left[5.373\,(1+\omega_i)\left(1 - \frac{T_{c,i}}{T}\right)\right]$$

It is a correlation, not a thermodynamic result, and its only role is to place the iteration in the right basin.

Convergence is measured on $\ln K$ rather than $K$, because $K$ spans many orders of magnitude in this mixture. Hydrogen and methanol at 78 bar and 313 K differ by roughly five decades, so an absolute tolerance on $K$ would be meaningless for one of them.

$$\max_i \left|\ln\frac{K_i^{(n+1)}}{K_i^{(n)}}\right| < \epsilon$$

## The trivial solution

Successive substitution has a fixed point that satisfies every equation and means nothing: $x_i = y_i = z_i$ with all $K_i = 1$. Both phases are identical, material balance holds, equilibrium holds, and the answer is physically empty.

An iteration that drifts toward this point produces $\psi$ values that look like a phase split but are not one. The guard tests for it directly:

$$\max_i \left|\ln K_i\right| < \epsilon_{\text{trivial}} \;\Longrightarrow\; \text{single phase}$$

When triggered, the result is labelled single phase and $\psi$ is set from the sign of $\sum_i z_i K_i^{\text{Wilson}} - 1$, which places the mixture as all vapour or all liquid to ensure ictitious split is not reported.

## Non-condensable components

Six of the nine species have no vapour pressure correlation because they are permanent gases at every condition in this flowsheet. On the $\gamma$-$\varphi$ path they need a $K_i$ nonetheless.

They return a large sentinel value, which drives $K_i = \gamma_i P^{\text{sat}}/P$ to a large number and places them essentially entirely in the vapour. This is a modelling device rather than a physical property, and it is the reason the $\gamma$-$\varphi$ path is restricted to low pressure where dissolved gas quantities are small. A rigorous treatment would use Henry's law constants, which are not available for this component set from the sources used here.

## Reported flags

The result carries more than the answer:

| Field | Meaning |
|---|---|
| `converged` | the outer loop met its tolerance |
| `single_phase` | the trivial-solution guard fired |
| `ideal_binary_pairs` | count of liquid pairs with no fitted NRTL parameters |

The last one exists because convergence is not the same as accuracy. A $\gamma$-$\varphi$ result can converge cleanly while treating carbon dioxide interactions as ideal, and a caller that cares about separation accuracy should be able to see that rather than infer it.

## Verification

The flash is verified through its consequence rather than in isolation, since a $K$ value for an arbitrary mixture has no published counterpart.

1. The bubble curve

    Covered in the activity coefficient document, checks the $\gamma$-$\varphi$ machinery against measured methanol and water equilibrium at eleven compositions, agreeing to within 0.008 in vapour mole fraction.

2. Regime coverage. 

    A representative reactor-outlet mixture flashed across 20 to 60 $^\circ\mathrm{C}$ and 5 to 75 bar converges at every point, selects $\gamma$-$\varphi$ below 10 bar and $\varphi$-$\varphi$ above it, and returns vapour fractions between 0.806 and 0.897 that vary smoothly with both temperature and pressure.

3. Material balance. 

    In the two-stage reactor, the inlet mass equals the stage-two outlet plus the interstage condensate to within $10^{-4}$ relative, which confirms the phase split conserves material through the flash and the stream reconstruction.

`flash.hpp` declares the result structure including the flags.

`flash.cpp` holds the Rachford-Rice residual and bisection, the Wilson initialisation, both $K$ formulations, and the trivial-solution guard.
