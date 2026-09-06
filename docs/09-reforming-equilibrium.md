# Reforming Equilibrium

A Gibbs-equilibrium extent solver for the tri-reforming reaction set, used as an equilibrium limit and as a cross-check on the kinetic reactor.

## Background: synthesis gas and the three reforming routes

Synthesis gas is a mixture of carbon monoxide and hydrogen, and it is the gateway intermediate for most of the chemicals made from methane. The route to it determines the $\mathrm{H_2}$ to $\mathrm{CO}$ ratio, and different downstream processes want different ratios: roughly 2 for methanol, 2 for Fischer-Tropsch, 3 for ammonia after shifting.

Three reactions convert methane to synthesis gas, and each has a characteristic ratio and a characteristic problem.

Steam methane reforming is the dominant industrial route. It is strongly endothermic, so it runs in fired tubes at 800 to 900 $^\circ\mathrm{C}$ with the heat supplied by burning part of the feed. It gives an $\mathrm{H_2}$ to $\mathrm{CO}$ ratio near 3, which is hydrogen-rich for methanol and requires either a $\mathrm{CO_2}$ import or a hydrogen export.

Dry reforming uses carbon dioxide as the oxidant instead of steam. It gives a ratio near 1, is even more endothermic, and is attractive on paper because it consumes $\mathrm{CO_2}$. Its industrial problem is carbon deposition: the low hydrogen content favours the Boudouard reaction $2\mathrm{CO} \rightarrow \mathrm{C} + \mathrm{CO_2}$ and methane cracking, both of which lay down coke that blocks the catalyst. Dry reforming alone is rarely run commercially for this reason.

Partial oxidation burns methane substoichiometrically. It is exothermic, needs no external fired duty, and gives a ratio near 2. Its problems are an oxygen supply, which usually means an air separation unit, and hot spots that damage catalyst and can run away to full combustion.

Tri-reforming, proposed by Song and Pan (2004), runs all three together in one bed. The combination is more than convenience:

- The exotherm from oxidation supplies the endotherm of the other two, so the reactor approaches thermal neutrality and needs no fired box.

- Steam and the water formed by oxidation suppress the coking that defeats dry reforming on its own.

- The three ratios blend, so the outlet can be tuned toward 2 by adjusting the feed rather than by shifting downstream.

- Carbon dioxide is consumed instead of being emitted.

The fit with this project is that the oxygen requirement, normally an air separation unit, is met by the electrolyser byproduct that would otherwise be vented.

### Equilibrium versus kinetics for this unit

At 800 $^\circ\mathrm{C}$ and above, reforming over a nickel catalyst is fast, and industrial reformers approach equilibrium closely enough that an equilibrium calculation predicts the outlet within a few percent. This is why the module described here solves for equilibrium rather than integrating rate laws, and why both source papers use an equilibrium reactor block.

Two methods exist for a multi-reaction equilibrium. Gibbs energy minimisation minimises total $G$ over all species subject to element balances, and needs no reaction set at all, which is what commercial RGibbs blocks do. Reaction extent picks a set of independent reactions and solves their equilibrium relations simultaneously for the extents. The second is used here because it is transparent and because the independent set is small, but it requires getting the independence question right, which the next section addresses.

## The reaction set

Tri-reforming combines three routes from methane to synthesis gas:

$$\mathrm{CH_4} + \mathrm{H_2O} \rightleftharpoons \mathrm{CO} + 3\,\mathrm{H_2} \qquad \Delta H_{298} = +206\ \mathrm{kJ\,mol^{-1}}$$

$$\mathrm{CH_4} + \mathrm{CO_2} \rightleftharpoons 2\,\mathrm{CO} + 2\,\mathrm{H_2} \qquad \Delta H_{298} = +247\ \mathrm{kJ\,mol^{-1}}$$

$$\mathrm{CH_4} + \tfrac12\,\mathrm{O_2} \rightarrow \mathrm{CO} + 2\,\mathrm{H_2} \qquad \Delta H_{298} = -36\ \mathrm{kJ\,mol^{-1}}$$

The first two are strongly endothermic; the third supplies heat. Running them together is what makes the process approximately thermally neutral without an external fired duty on the reactor itself.

## Independence

Four reactions are commonly written for this system, adding the water gas shift:

$$\mathrm{CO} + \mathrm{H_2O} \rightleftharpoons \mathrm{CO_2} + \mathrm{H_2}$$

They are not linearly independent. Subtracting dry reforming from steam reforming gives

$$(\mathrm{CH_4} + \mathrm{H_2O}) - (\mathrm{CH_4} + \mathrm{CO_2}) \;\rightarrow\; (\mathrm{CO} + 3\mathrm{H_2}) - (2\mathrm{CO} + 2\mathrm{H_2})$$

$$\mathrm{H_2O} - \mathrm{CO_2} \rightarrow -\mathrm{CO} + \mathrm{H_2} \qquad\text{that is}\qquad \mathrm{CO} + \mathrm{H_2O} \rightarrow \mathrm{CO_2} + \mathrm{H_2}$$

which is the shift. Two extents therefore span the full composition space reachable by all three reforming reactions plus shift, and solving for two is sufficient.

Cho et al. state this directly for their own reaction set, noting that the four reactions are not linearly independent and that one is a combination of the others. Aboosadi et al. make the same observation. Both nonetheless carry four reactions in their kinetic models, because the rate expressions were fitted individually and using all four preserves the fitted forms. That is a kinetic argument, not a stoichiometric one, and it does not apply to an equilibrium calculation where only the reachable composition space matters.

## Partial oxidation as a pre-step

Oxygen is consumed to completion in the first fraction of the bed and its reaction is effectively irreversible at reforming temperatures. It is therefore applied before the equilibrium solve as a limiting-reagent conversion.

$$\xi_{\mathrm{POM}} = \min\!\left(n_{\mathrm{CH_4}},\; 2\,n_{\mathrm{O_2}}\right)$$

The factor of two follows from the stoichiometry: each extent consumes half a mole of oxygen, so exhausting $n_{\mathrm{O_2}}$ corresponds to an extent of $2 n_{\mathrm{O_2}}$.

$$n_{\mathrm{CH_4}} \mathrel{-}= \xi, \quad
n_{\mathrm{O_2}} \mathrel{-}= \tfrac12\xi, \quad
n_{\mathrm{CO}} \mathrel{+}= \xi, \quad
n_{\mathrm{H_2}} \mathrel{+}= 2\xi$$

Note this is partial oxidation to carbon monoxide and hydrogen, not complete combustion to carbon dioxide and water. The distinction matters for the energy balance: partial oxidation releases 36 $\mathrm{kJ\,mol^{-1}}$ while complete combustion releases 803 $\mathrm{kJ\,mol^{-1}}$, a factor of twenty-two. Which one is physically correct depends on the catalyst and the oxygen-to-methane ratio, and the choice made here follows the reaction as written in the source papers.

## The equilibrium solve

With the post-combustion composition as the starting point, two extents remain.

Applying both:

$$n_{\mathrm{CH_4}} \mathrel{-}= \xi_1 + \xi_2, \quad n_{\mathrm{H_2O}} \mathrel{-}= \xi_1, \quad n_{\mathrm{CO_2}} \mathrel{-}= \xi_2$$

$$n_{\mathrm{CO}} \mathrel{+}= \xi_1 + 2\xi_2, \quad n_{\mathrm{H_2}} \mathrel{+}= 3\xi_1 + 2\xi_2$$

The equilibrium conditions are that each reaction quotient equals its equilibrium constant:

$$K_{\mathrm{SRM}} = \frac{p_{\mathrm{CO}}\,p_{\mathrm{H_2}}^{3}}{p_{\mathrm{CH_4}}\,p_{\mathrm{H_2O}}},
\qquad
K_{\mathrm{DRM}} = \frac{p_{\mathrm{CO}}^{2}\,p_{\mathrm{H_2}}^{2}}{p_{\mathrm{CH_4}}\,p_{\mathrm{CO_2}}}$$

with $p_i = P\,n_i / \sum_j n_j$ and $P$ in bar, because the equilibrium constants come from Gibbs energy and are referenced to a 1 bar standard state. Both reactions have $\Delta n = 2$, so using pascals would introduce a factor of $10^{10}$.

### Residuals in logarithmic form

The two residuals are written as

$$R_1 = \ln K_{\mathrm{SRM}}(T) - \ln Q_{\mathrm{SRM}}, \qquad R_2 = \ln K_{\mathrm{DRM}}(T) - \ln Q_{\mathrm{DRM}}$$

rather than as $K - Q$. The reaction quotients span many orders of magnitude across the extents and temperatures encountered during the iteration, so a raw difference is badly scaled and Newton's method handles it poorly. The logarithmic form keeps both residuals of order unity.

## Solution method

Two-dimensional Newton-Raphson with a numerical Jacobian.

$$\mathbf{J} = \begin{bmatrix} \partial R_1/\partial \xi_1 & \partial R_1/\partial \xi_2 \partial R_2/\partial \xi_1 & \partial R_2/\partial \xi_2 \end{bmatrix}, \qquad \mathbf{J}\,\boldsymbol{\delta} = -\mathbf{R}$$

solved by Cramer's rule, which is the standard closed form for a two by two system.

The Jacobian is evaluated by finite difference and not analytically. Differentiating the logarithmic residuals through the extent application and the total-moles normalisation is possible but error-prone to transcribe, and for a two by two system evaluated at most a few hundred times the cost of two extra residual evaluations per iteration is not significant.

### Feasibility damping

A full Newton step can drive a species mole number negative, which makes the next residual evaluation take the logarithm of a negative quantity. Each step is therefore damped by the largest factor that keeps every depleted species non-negative:

$$\alpha = 0.9 \cdot \min_i \left\{ \frac{-n_i}{\Delta n_i} : \Delta n_i < 0 \right\}, \qquad \alpha \le 1$$

with the 0.9 margin placing the step strictly inside the feasible region and not on its boundary. Only three species are depleted by the extents, methane by both reactions, water by steam reforming and carbon dioxide by dry reforming, so only three limits need checking.

## Verification

Against methane conversion reported by two independent papers, at a feed of $\mathrm{CH_4}:\mathrm{H_2O}:\mathrm{CO_2}:\mathrm{O_2} = 1:1:0.5:0.15$ and 25 bar.

| Condition | Computed | Reference |
|---|---|---|
| 1046 $^\circ\mathrm{C}$ | 96.14 % | 97 % (Shi 2020) |
| 1100 $^\circ\mathrm{C}$ | 98.16 % | 99 % (Lim 2022) |

Methane conversion near the equilibrium limit is insensitive to the exact feed ratio, which is why it can be compared against papers that used a different one. Carbon dioxide conversion is not, so it is reported as informational rather than as a validated comparison.

The reaction enthalpies underlying the equilibrium constants are verified separately against both papers, agreeing to better than 1 %.

## Relationship to the kinetic reactor

This module has no catalyst mass coordinate and no rate expression. It answers what the composition would be if the reactions ran to completion, which is a different question from what a finite reactor produces.

The kinetic tri-reforming reactor, documented separately, integrates real rate laws over a bed. The two are complementary: this module supplies the equilibrium limit that the kinetic result must not exceed.

`trm_kinetics.hpp` declares the feed and result structures and the two equilibrium constants. 

`trm_kinetics.cpp` holds the combustion pre-step, the extent application, the logarithmic residuals, the damped Newton iteration
and the conversion helpers.
