# Methanol Synthesis Kinetics

The Vanden Bussche and Froment rate expressions with the Mignard and Pritchard parameters, and one place where the implementation departs from its cited source.

## The reaction set

Two independent reactions on a copper, zinc oxide and alumina catalyst:

$$\mathrm{CO_2} + 3\,\mathrm{H_2} \rightleftharpoons \mathrm{CH_3OH} + \mathrm{H_2O} \qquad \Delta H_{298} = -49.3\ \mathrm{kJ\,mol^{-1}}$$

$$\mathrm{CO_2} + \mathrm{H_2} \rightleftharpoons \mathrm{CO} + \mathrm{H_2O} \qquad \Delta H_{298} = +41.2\ \mathrm{kJ\,mol^{-1}}$$

Carbon monoxide hydrogenation is the difference of these two and is therefore not independent. Two reactions span the space.

Both reaction enthalpies are gas basis. Van-Dal reports −87 $\mathrm{kJ\,mol^{-1}}$ for the first, which is on a liquid methanol basis and cannot be used in a gas-phase energy balance.

## Background: heterogeneous catalytic rate laws

A homogeneous reaction rate is a power law in concentration, because collisions in a fluid scale that way. A heterogeneous catalytic reaction does not, because reaction happens on a finite number of surface sites and those sites can saturate. Doubling a reactant partial pressure doubles the rate only while the surface is sparsely covered. Once it is crowded, further pressure does nothing, and if the extra species adsorbs strongly it can displace a co-reactant and slow the reaction outright.

Langmuir-Hinshelwood-Hougen-Watson kinetics is the standard framework for this. The construction has three steps.

Adsorption equilibrium. Each species adsorbs reversibly on a site, and the fractional coverage follows the Langmuir isotherm:

$$\theta_i = \frac{K_i p_i}{1 + \sum_j K_j p_j}$$

$K_i$ is an adsorption equilibrium constant. Because adsorption is exothermic, $K_i$ decreases with temperature, so adsorption terms carry negative apparent activation energies.

A rate-determining surface step. One elementary step is assumed slow enough to control the overall rate, and all others are taken to be at equilibrium. The rate is written for that step in terms of the coverages of its participants.

Substitution. Replacing coverages with the isotherm expressions produces the characteristic LHHW shape:

$$r = \frac{(\text{kinetic term})(\text{driving force})}{(\text{adsorption term})^n}$$

Each factor is interpretable, and reading a published rate law this way is faster than parsing it algebraically:

- The kinetic term is the rate constant of the controlling step, Arrhenius in temperature.

- The driving force contains the approach to equilibrium. It is written as $1 - Q/K_{eq}$ so that it vanishes when the reaction quotient reaches the equilibrium constant, and it changes sign if the reaction runs backwards. This is what makes the expression thermodynamically consistent rather than a fit that would keep producing product past equilibrium.

- The adsorption term in the denominator is the site balance. The exponent $n$ is the number of sites involved in the controlling step, so a cubed denominator implies three sites.

The denominators below are cubed for synthesis and first power for the shift reaction, which encodes a mechanistic claim about how many sites each step occupies. That claim came from fitting, not from independent measurement, and this is generally true of LHHW parameters: they are regressed on rate data, so the mechanism is a hypothesis the fit is consistent with.

### Why carbon dioxide is written as the carbon source

For decades the assumed route was $\mathrm{CO}$ hydrogenation, with $\mathrm{CO_2}$ contributing only through the shift reaction. Isotopic labelling work in the 1980s and 1990s, notably Chinchen and coworkers feeding $^{14}\mathrm{C}$-labelled $\mathrm{CO_2}$, showed the opposite: the carbon in methanol comes predominantly from $\mathrm{CO_2}$ even in $\mathrm{CO}$-rich feeds. $\mathrm{CO}$'s main role is to scavenge surface oxygen through the reverse shift, keeping the copper reduced and the sites free.

The rate law below reflects that finding, which is why $p_{\mathrm{CO_2}}$ and not $p_{\mathrm{CO}}$ appears in the numerator of the synthesis rate. It also explains why the model is applicable to a pure $\mathrm{CO_2}$ feed at all, which a $\mathrm{CO}$-based rate law would not be.

## Rate expressions

Vanden Bussche and Froment (1996), a Langmuir-Hinshelwood-Hougen-Watson form derived from a mechanism in which carbon dioxide is the direct carbon source and the surface is dominated by adsorbed oxygen and hydroxyl.

$$r_{\mathrm{CH_3OH}} = \frac{k_1\,p_{\mathrm{CO_2}}\,p_{\mathrm{H_2}} \left[1 - \dfrac{1}{K_{eq,1}}\dfrac{p_{\mathrm{H_2O}}\,p_{\mathrm{CH_3OH}}}{p_{\mathrm{H_2}}^{3}\,p_{\mathrm{CO_2}}}\right]}{D^{3}}$$

$$r_{\mathrm{RWGS}} = \frac{k_5\,p_{\mathrm{CO_2}} \left[1 - K_{eq,2}\dfrac{p_{\mathrm{H_2O}}\,p_{\mathrm{CO}}}{p_{\mathrm{CO_2}}\,p_{\mathrm{H_2}}}\right]}{D}$$

sharing one adsorption denominator:

$$D = 1 + k_2\,\frac{p_{\mathrm{H_2O}}}{p_{\mathrm{H_2}}} + k_3\sqrt{p_{\mathrm{H_2}}} + k_4\,p_{\mathrm{H_2O}}$$

Rates are in $\mathrm{mol\,kg_{cat}^{-1}\,s^{-1}}$ and partial pressures in bar.

Three features follow directly from the algebra.

The bracket in each numerator is the approach to equilibrium. It equals one far from equilibrium and falls to zero at it, and it goes negative if the reaction is driven backwards, which is the correct behaviour and is why the rate function must not clamp it.

The denominator is cubed for methanol and linear for the shift. The exponent is the number of surface sites the rate-determining step requires, and it is a result of the mechanism rather than a fitting parameter.

The $p_{\mathrm{H_2O}}/p_{\mathrm{H_2}}$ term is water inhibition. Water produced by the reaction competes for surface sites, so conversion is self-limiting beyond the equilibrium constraint alone. This is the reason interstage condensation raises overall conversion.

## Parameters

Each constant follows $k_i = A_i \exp(B_i / RT)$ with $B$ in $\mathrm{J\,mol^{-1}}$. Note the sign convention: $B$ is positive inside the exponent, so an adsorption constant that strengthens as temperature falls carries a positive $B$.

| Constant | $A$ | $B$ ($\mathrm{J\,mol^{-1}}$) |
|---|---|---|
| $k_1$ | 1.07 | 40,000 |
| $k_2$ | 3453.38 | 0 |
| $k_3$ | 0.499 | 17,197 |
| $k_4$ | $6.62\times10^{-11}$ | 124,119 |
| $k_5$ | $1.22\times10^{10}$ | −98,084 |

Source: Van-Dal and Bouallou (2013) Table 3.

### Why these are not the original values

The rate expressions are Vanden Bussche and Froment's. The parameters are not their originals. Van-Dal Section 2.3.2 states that the activation energies were readjusted by Mignard and Pritchard (2008) to better represent the experimental data, and that the refit extended the application range of the model to 75 bar.

So $B_1 = 40{,}000$ here against 36,696 in the 1996 paper, and $B_5 = -98{,}084$ against −94,765. These are the refitted values and they are the correct choice for this model, because the synthesis loop runs at 78 bar, outside the range the original parameters were regressed over.

This has been queried more than once when comparing against other implementations, which is why it is recorded here and in the source file.

## Equilibrium constants

Graaf et al. (1986), quoted by Van-Dal as Equations 8 and 9:

$$\log_{10} K_{eq,1} = \frac{3066}{T} - 10.592 \qquad [\mathrm{bar^{-2}}]$$

$$\log_{10} K_{eq,2} = \frac{2073}{T} - 2.029 \qquad [\text{dimensionless}]$$

These are used instead of the values computed from Gibbs energy, because the rate expressions were regressed with these correlations in place. Substituting a thermodynamically derived constant decouples the approach-to-equilibrium bracket from what the authors actually fitted, and the rate no longer vanishes exactly at the equilibrium the correlation defines.

### A deliberate departure from the printed equation

Van-Dal's Equation 9 is printed as

$$\log_{10} K_{eq,2} = \frac{-2073}{T} + 2.029$$

with both signs opposite to the form implemented here. The implementation is deliberate and the paper contains a typographical error. Three lines of evidence.

Physics. Van-Dal's Equation 6 writes the reverse shift driving force as $\left[1 - K_{eq,2}\,p_{\mathrm{H_2O}}p_{\mathrm{CO}}/(p_{\mathrm{CO_2}}p_{\mathrm{H_2}})\right]$. For that bracket to vanish at equilibrium, $K_{eq,2}$ must be the forward water gas shift constant, the reciprocal of the reverse shift constant. At 493.15 K the water gas shift constant is between 130 and 150. As printed, Equation 9 gives $6.7\times10^{-3}$. As implemented, it gives 149.5. Only the second is a physical shift constant, and only the second makes the driving force approach equilibrium.

Provenance. Van-Dal attributes both equations to Graaf et al. (1986), whose shift correlation is $\log_{10} K = 2073/T - 2.029$, exactly the implemented form. Equation 8 is reproduced from Graaf with its signs intact; Equation 9 is not.

Internal consistency. The two constants come from the same source and the same table.

The test suite labels this check as the Graaf form rather than as Van-Dal Equation 9, so a reader cross-checking against the paper is not misled.

## Verification

At 493.15 K, which is Van-Dal's stated reactor inlet temperature.

| Quantity | Computed | Reference |
|---|---|---|
| $K_{eq,1}$ | $4.2187\times10^{-5}$ | Graaf correlation |
| $K_{eq,2}$ | 149.48 | shift constant, literature 130 to 150 |
| $r_{\mathrm{CH_3OH}}$ | 0.117662 | regression pin |
| $r_{\mathrm{RWGS}}$ | 0.003510 | regression pin |

in $\mathrm{mol\,kg_{cat}^{-1}\,s^{-1}}$ at $P = 50$ bar with $y_{\mathrm{CO_2}} = 0.03$, $y_{\mathrm{H_2}} = 0.82$, $y_{\mathrm{CO}} = 0.04$ and trace water and methanol.

The equilibrium constants are checked against their published correlations and against an independent physical band. The two rate values are labelled as regression pins: they are this implementation's own output at a stated composition, and they guard against accidental change rather than proving correctness. The consequential check on the rate expressions is the reactor integration reproducing Van-Dal's per-pass conversion, which is covered in the reactor integration document.

A sign check is also asserted directly: away from equilibrium the reverse shift rate must be positive, which fails immediately if the driving force bracket is inverted.

`lhhw.hpp` declares the two rate functions and the two equilibrium constants. 

`lhhw.cpp` holds the parameters with the Mignard and Pritchard note and the $K_{eq,2}$ departure with its evidence.
