# Equation of State

Peng-Robinson for real-gas density and fugacity coefficients, with mixing rules and binary interaction parameters for the synthesis loop.

## Background: cubic equations of state

The ideal gas law treats molecules as points that do not interact. Real molecules occupy volume and attract one another, exerting opposing effects on pressure: finite volume increases it, whereas intermolecular attraction decreases it. Van der Waals (1873) wrote the first equation of state that carried both:

$$P = \underbrace{\frac{RT}{V_m - b}}_{\text{repulsion}} - \underbrace{\frac{a}{V_m^2}}_{\text{attraction}}$$

The repulsive term says molecules cannot be compressed below their own finite molecular volume $b$, so pressure diverges as $V_m \to b$. The attractive term subtracts from pressure because molecules pulling on one another strike the walls with less force, and it scales as $V_m^{-2}$ because the number of interacting pairs per unit volume scales with the square of density.

Two properties made this form dominant in process engineering and keep it dominant now. First, it is cubic in volume, so its roots are analytic and computationally cheap, which matters when an equation of state is evaluated inside a flash calculation nested within an ODE integrator. Second, and less obvious, below the critical temperature it returns three real roots, and the largest and smallest of them are the vapour and liquid molar volumes respectively. One equation therefore describes both phases and can predict the equilibrium between them, which no separate correlation for each phase can do.

Van der Waals is quantitatively poor. The line of improvements that followed all kept the structure and modified the attraction term:

| Equation | Change | Effect |
|---|---|---|
| Redlich-Kwong (1949) | $a/V_m^2 \to a/[T^{0.5}V_m(V_m+b)]$ | Vapour densities become usable |
| Soave (1972) | $T^{-0.5} \to \alpha(T,\omega)$ fitted to vapour pressure | Vapour pressures become accurate |
| Peng-Robinson (1976) | denominator $\to V_m^2 + 2bV_m - b^2$ | Liquid densities improve |

Soave's step is the conceptual one. Rather than assume a temperature dependence, he fitted $\alpha(T)$ so the equation reproduces measured vapour pressures along the saturation curve. A flash calculation needs an equation calibrated on vapour pressure because it predicts phase equilibrium well.

Peng and Robinson changed the denominator to improve the predicted critical compressibility. Van der Waals gives $Z_c = 0.375$ and Redlich-Kwong 0.333, against 0.24 to 0.29 for real substances. Peng-Robinson gives 0.307, still high but closer, and the improvement shows up as better liquid density. That is why Peng-Robinson is the usual choice when a liquid phase is present, and Soave-Redlich-Kwong when the system is gas-dominated.

### The acentric factor

Two-parameter corresponding states holds that all fluids follow one reduced equation once scaled by $T_c$ and $P_c$. It works for argon, krypton and xenon and fails for everything else, because real molecules are not spherical and may carry a dipole. Pitzer (1955) captured the deviation in one number, defined from the reduced vapour pressure at 70 percent of the critical temperature:

$$\omega = -\log_{10}\!\left(P^{\text{sat}}_r\right)_{T_r = 0.7} - 1.000$$

The offset is chosen so $\omega \approx 0$ for the noble gases. Values rise with molecular elongation and polarity: 0.012 for methane, 0.224 for carbon dioxide, 0.345 for water, 0.566 for methanol. In a cubic equation $\omega$ enters through $\kappa$, which sets how fast $\alpha$ falls with temperature. Because $\kappa$ comes from a polynomial regressed over a finite range of $\omega$, a component far outside that range is being handled by an extrapolated correlation.

### Fugacity, and why the equation of state is needed at all

Phase equilibrium is the equality of chemical potential across phases. Chemical potential is inconvenient because it diverges logarithmically as composition goes to zero, so Lewis introduced fugacity $f$, a pressure-like quantity defined so that

$$\mu_i - \mu_i^{\circ} = RT \ln\frac{f_i}{f_i^{\circ}}, \qquad f_i \to y_i P \ \text{ as } P \to 0$$

Fugacity is the effective pressure a real component exerts once non-ideality is accounted for. Equality of chemical potential becomes equality of fugacity:

$$f_i^V = f_i^L$$

The fugacity coefficient $\phi_i = f_i/(y_i P)$ measures the departure from ideal gas behaviour, and it is obtainable from any equation of state through a volumetric integral. That is the whole reason a cubic equation appears in this project: it converts $P$-$V$-$T$ behaviour into the $\phi_i$, and that is used to decide how much methanol leaves the loop as liquid.

## Why a cubic equation is needed here

The synthesis loop runs at 78 bar. At that pressure the compressibility factor departs from ideal behaviour by enough to matter for two quantities: the molar density that sets the mass flux in the pressure drop correlation, and the fugacity coefficients that set the phase split in the flash.

The Peng-Robinson equation (Peng and Robinson 1976) is used rather than Soave-Redlich-Kwong or a Helmholtz reference equation. Methanol and water condense in this process, so liquid density accuracy matters, which favours Peng-Robinson. Its parameters are available for every species here, and a Helmholtz reference equation of the GERG type would be more accurate for the light gases but has no parameters for methanol in a mixture of this composition.

$$P = \frac{RT}{V_m - b} - \frac{a\,\alpha(T)}{V_m^2 + 2bV_m - b^2}$$

## Pure component parameters

$$a = 0.45724\ \frac{R^2 T_c^2}{P_c}\ \alpha(T), \qquad b = 0.07780\ \frac{R T_c}{P_c}$$

$$\alpha(T) = \left[1 + \kappa\left(1 - \sqrt{T/T_c}\right)\right]^2$$

The $\kappa$ correlation depends on acentric factor, here the original 1976 form is not sufficient for this component set.

$$\kappa = \begin{cases} 0.37464 + 1.54226\,\omega - 0.26992\,\omega^2 & \omega \le 0.49 \\[4pt] 0.379642 + 1.48503\,\omega - 0.164423\,\omega^2 + 0.016666\,\omega^3 & \omega > 0.49 \end{cases}$$

The upper branch is Robinson and Peng's own extension (GPA Research Report RR-28, 1978), published for heavier and more polar components than the 1976 correlation was regressed on.

Methanol requires this. Its acentric factor is 0.566, beyond the stated validity limit of 0.49. Using the 1976 form for methanol applies a correlation outside its range to the most important condensable species in the process, and therefore to every fugacity coefficient that governs how much product is recovered. The two branches are continuous to within $10^{-4}$ at the switchover, so no discontinuity is introduced for species near the boundary.

## Mixing rules

A cubic equation is written for a pure substance. Applying it to a mixture requires rules for what $a$ and $b$ mean when several species are present. The van der Waals one-fluid approach treats the mixture as a hypothetical pure fluid whose parameters are composition averages:

$$a_{\text{mix}} = \sum_i \sum_j y_i y_j \sqrt{a_i a_j}\,(1 - k_{ij}), \qquad b_{\text{mix}} = \sum_i y_i b_i$$

The volume parameter is a linear average because excluded volumes add. The attraction parameter is quadratic because attraction is a pairwise interaction, and the cross term uses a geometric mean, which follows from London dispersion theory for the interaction between unlike molecules.

The geometric mean is exact only for molecules of similar size and polarity. The correction factor $k_{ij}$ absorbs the error, and it is fitted to binary vapour-liquid equilibrium data. Values are typically within $\pm 0.15$ of zero. Sign carries meaning: positive $k_{ij}$ weakens the predicted cross attraction, which is usual for pairs of dissimilar polarity, while negative values indicate stronger attraction than the geometric mean suggests, as with hydrogen and carbon dioxide below.

A binary interaction parameter is not a physical constant. It belongs to the equation of state, the mixing rule and the data set it was fitted against, so a $k_{ij}$ taken from one correlation cannot be transferred to another.

## Binary interaction parameters

Source: the ChemSep binary interaction databank (Kooijman and Taylor, distributed under the Artistic Licence with COCO and ChemSep), Peng-Robinson table, itself drawn from DECHEMA.

ChemSep contains exactly seventeen pairs among these nine species. All seventeen were verified against the databank to five decimal places.

| Pair | $k_{ij}$ | Fitted range |
|---|---|---|
| $\mathrm{H_2}$ / $\mathrm{N_2}$ | +0.0711 | 90 to 113 K |
| $\mathrm{H_2}$ / $\mathrm{CO}$ | +0.0919 | 68 to 88 K |
| $\mathrm{H_2}$ / $\mathrm{CH_4}$ | −0.0044 | 103 to 174 K |
| $\mathrm{H_2}$ / $\mathrm{CO_2}$ | −0.1622 | not stated |
| $\mathrm{N_2}$ / $\mathrm{CO}$ | +0.0300 | 70 to 122 K |
| $\mathrm{N_2}$ / $\mathrm{Ar}$ | −0.0004 | 81 to 115 K |
| $\mathrm{N_2}$ / $\mathrm{O_2}$ | −0.0159 | 91 to 125 K |
| $\mathrm{N_2}$ / $\mathrm{CH_4}$ | +0.0289 | 89 to 189 K |
| $\mathrm{N_2}$ / $\mathrm{CO_2}$ | −0.0122 | 233 to 293 K |
| $\mathrm{N_2}$ / $\mathrm{CH_3OH}$ | −0.2141 | not stated |
| $\mathrm{CO}$ / $\mathrm{CH_4}$ | +0.0300 | not stated |
| $\mathrm{Ar}$ / $\mathrm{O_2}$ | +0.0089 | 91 to 120 K |
| $\mathrm{Ar}$ / $\mathrm{CH_4}$ | +0.0152 | 123 to 164 K |
| $\mathrm{CH_4}$ / $\mathrm{CO_2}$ | +0.0978 | 200 to 271 K |
| $\mathrm{CO_2}$ / $\mathrm{CH_3OH}$ | +0.0583 | 298 to 313 K |
| $\mathrm{CO_2}$ / $\mathrm{H_2O}$ | +0.0952 | 383 to 623 K |
| $\mathrm{CH_3OH}$ / $\mathrm{H_2O}$ | −0.0778 | not stated |

### Two limitations


Coverage is not complete. Pairs absent from the databank fall through to $k_{ij} = 0$, which is the van der Waals default. The absent pairs include $\mathrm{H_2}/\mathrm{H_2O}$, $\mathrm{H_2}/\mathrm{CH_3OH}$ and $\mathrm{CO}/\mathrm{CH_3OH}$. Hydrogen and methanol in particular is a pair the synthesis loop sees at 78 bar, so phase-split results involving it carry more uncertainty than the parameterised pairs.


Several are used outside their fitted range. Most of these parameters were regressed on cryogenic or near-ambient data, while the synthesis loop runs near $500 \ \mathrm{K}$. $\mathrm{CO_2}/\mathrm{CH_3OH}$ was fitted over $298 \ \mathrm{K}$ to $313 \ \mathrm{K}$ and is used at $500 \ \mathrm{K}$. These ranges are in the validity module so that a design point which uses them gets flagged.

## Solving for compressibility

Recasting in terms of the compressibility factor $Z = PV_m/RT$ is standard practice. $Z$ is dimensionless, it measures the deviation from ideality directly since $Z = 1$ is the ideal gas, and the resulting polynomial has coefficients built from two dimensionless groups instead of being built from $a$, $b$, $P$ and $T$ separately.

With $A = a_{\text{mix}}P/(R^2T^2)$ and $B = b_{\text{mix}}P/(RT)$:

$$Z^3 - (1-B)Z^2 + \left(A - 3B^2 - 2B\right)Z - \left(AB - B^2 - B^3\right) = 0$$

Solved by Cardano's method rather than by iteration. Substituting $Z = x - c_2/3$ removes the quadratic term and leaves a depressed cubic $x^3 + px + q = 0$ with discriminant $\Delta = q^2/4 + p^3/27$.

When $\Delta > 0$ there is one real root, meaning a single fluid phase is possible at that condition. When $\Delta \le 0$ there are three, and the trigonometric form is used:

$$x_k = 2\sqrt{-p/3}\,\cos\!\left(\frac{\phi + 2\pi k}{3}\right), \quad \phi = \arccos\!\left(\frac{-q/2}{\sqrt{-p^3/27}}\right), \quad k = 0,1,2$$

Roots with $Z \le B$ are discarded as unphysical, since $B$ corresponds to the molar co-volume. The largest surviving root is the vapour, the smallest the liquid.

When three roots appear, the middle one is always discarded. It lies on the branch where $(\partial P/\partial V)_T > 0$, meaning the fluid would expand as pressure rises, which violates mechanical stability. It is an artifact of forcing a continuous analytic function through a region where two phases coexist, and it has no physical counterpart.

## Fugacity coefficients

For a cubic equation the volumetric integral for $\ln\phi_i$ can be evaluated in closed form, which is another reason cubic equations are used. 

Peng-Robinson gives:

$$\ln\phi_i = \frac{b_i}{b_{\text{mix}}}(Z-1) - \ln(Z-B) - \frac{A}{2\sqrt{2}B}\left[\frac{2\sum_j y_j a_{ij}}{a_{\text{mix}}} - \frac{b_i}{b_{\text{mix}}}\right] \ln\!\left[\frac{Z + (1+\sqrt{2})B}{Z - (\sqrt{2}-1)B}\right]$$

with $a_{ij} = \sqrt{a_i a_j}(1-k_{ij})$.

These feed the $\varphi$-$\varphi$ flash, where the equilibrium ratio is

$$K_i = \frac{\phi_i^L}{\phi_i^V}$$

## Verification

Compressibility and fugacity coefficients are not directly comparable against a published table for an arbitrary mixture, so verification is structural and through consequence and not by direct lookup.

The parameters themselves were confirmed against the ChemSep databank, all seventeen $k_{ij}$ matching to five decimals. Critical constants were confirmed against an independent compilation. The equation reduces correctly in its limits: at low pressure $Z \to 1$ and $\phi_i \to 1$, and for a single component the mixture rules reduce to the pure parameters.

The consequential check is the flash, where the equation of state determines the phase split. That is covered in the flash document.

`eos.hpp` declares the parameter structures and the interface.

`eos.cpp` holds the $\kappa$ branches, the $k_{ij}$ table, the Cardano solver, and the fugacity coefficient expression.

