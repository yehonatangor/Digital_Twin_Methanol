# Equipment Costing

Purchased and installed cost of individual equipment items from a published correlation, the geometry calculations that supply its size argument, and the index escalation that moves a cost between years.

## Background: why equipment costing is a correlation problem

A chemical plant's capital cost is dominated by a few dozen items of equipment, and the cost of each one is a smooth function of a single characteristic size. A heat exchanger scales with area, a vessel with volume, a compressor with shaft power. Within a class of equipment the relationship is close to a power law, because material and fabrication effort scale with surface area while capacity scales with volume, and the two grow at different rates. That is the origin of the familiar six-tenths rule,

$$\frac{C_2}{C_1} = \left(\frac{A_2}{A_1}\right)^{n}, \qquad n \approx 0.6$$

A single exponent is too coarse over a wide size range, so published cost data is usually fitted as a quadratic in logarithms instead, which allows the local exponent to vary. That is the form used throughout this chapter.

Three further adjustments stand between a purchased cost and money actually spent.

Material. The correlation is fitted for carbon steel. Stainless steel, nickel alloys and exotic materials multiply the cost of the pressure-containing parts.

Pressure. Thicker walls cost more, and above a threshold the relationship is steep because fabrication and inspection requirements change as well.

Installation. Buying a vessel is a fraction of the cost of having one operating. Foundations, structural steel, piping, instrumentation, electrical work, insulation, painting, labour, freight, insurance, taxes, engineering and contractor fees are all real and all roughly proportional to the purchased cost of the item they serve. Bundling them into a single multiplier on purchased cost gives the bare-module cost, and that multiplier is the largest single number in the whole calculation.

A cost estimate is only meaningful with a date attached. Equipment prices move with the cost of steel, labour and energy, so published correlations are tied to a base year and escalated with a published index.

## Sources, and why they are kept separate

Three sources contribute, and they are deliberately not blended.

| Source | What is taken from it |
|---|---|
| Turton et al., 5th ed., 2018, Ch. 7 and App. A | The entire per-equipment costing method used internally |
| Lim et al., *Renew. Sustain. Energy Rev.* 155 (2022) 111876 | Annualisation and prices, covered in the next chapter |
| Mucci et al., *J. Energy Storage* 72 (2023) 108614, App. A.5 | A six-tenths anchor used only as a cross-check |

Mucci's capital cost comes from a different method, Biegler's 1.85 and 6.15 multipliers, and is expressed in a different currency on a different equipment scope. Applying Mucci's multiplier on top of Turton's bare-module factor would count the same installation costs twice, because $F_{BM}$ already contains what Biegler's 1.85 covers. The two are therefore never combined. Only Turton's own total-module and grassroots formulas are used in the calculation, and Mucci's anchor is exposed separately so a reader can compare an answer against it.

## Purchased cost

Turton Eq. (A.1), fitted to 2001 US dollars at a Chemical Engineering Plant Cost Index of 397:

$$\log_{10} C_p^0 = K_1 + K_2 \log_{10} A + K_3 \left(\log_{10} A\right)^2$$

$A$ is the characteristic size, and its units differ by equipment class. The constants are Table A.1. All nine rows used here, 45 constants, were checked character by character against the printed table.

| Equipment | $K_1$ | $K_2$ | $K_3$ | Size, units | Range |
|---|---|---|---|---|---|
| Exchanger, floating head | 4.8306 | −0.8509 | 0.3187 | Area, $\mathrm{m^{2}}$ | 10 to 1000 |
| Exchanger, fixed tube | 4.3247 | −0.3030 | 0.1634 | Area, $\mathrm{m^{2}}$ | 10 to 1000 |
| Exchanger, U-tube | 4.1884 | −0.2503 | 0.1974 | Area, $\mathrm{m^{2}}$ | 10 to 1000 |
| Exchanger, kettle reboiler | 4.4646 | −0.5277 | 0.3955 | Area, $\mathrm{m^{2}}$ | 10 to 100 |
| Exchanger, double pipe | 3.3444 | 0.2745 | −0.0472 | Area, $\mathrm{m^{2}}$ | 1 to 10 |
| Process vessel, horizontal | 3.5565 | 0.3776 | 0.0905 | Volume, $\mathrm{m^{3}}$ | 0.1 to 628 |
| Process vessel, vertical | 3.4974 | 0.4485 | 0.1074 | Volume, $\mathrm{m^{3}}$ | 0.3 to 520 |
| Sieve tray | 2.9949 | 0.4465 | 0.3961 | Area, $\mathrm{m^{2}}$ per tray | 0.07 to 12.30 |
| Compressor | 2.2897 | 1.3604 | −0.1027 | Fluid power, kW | 450 to 3000 |

A worked check of the form itself: at $A = 1$ every logarithmic term vanishes and the cost collapses to $10^{K_1}$. For a floating-head exchanger that is $10^{4.8306} = 67{,}702$ dollars, which the test suite pins exactly.

The size ranges are recorded but not enforced. Extrapolating a quadratic in logarithms outside its fitted range is a decision the caller should make knowingly, and clamping the size would hide it.

Towers have no row of their own. A distillation tower is costed as a vertical process vessel, which is Turton's own convention, with the trays costed separately per tray.

Packed-bed reactors are costed as shell-and-tube exchangers rather than through Table A.1's own "Reactors" row, following Turton Appendix B.5. A multitubular methanol converter is physically a tube bundle in a shell, and the exchanger correlation is fitted to that geometry.

## Pressure factor

For most equipment, Turton Eq. (A.3):

$$\log_{10} F_P = C_1 + C_2 \log_{10} P + C_3 \left(\log_{10} P\right)^2$$

with $P$ in bar gauge. Table A.2's own convention is that all-zero constants mean $F_P = 1$, which is how the correlation expresses "no pressure penalty in this range". The implementation reproduces that convention rather than evaluating $10^0$ and arriving at the same answer by accident.

| Equipment | $C_1$ | $C_2$ | $C_3$ | Range |
|---|---|---|---|---|
| Heat exchanger | 0.03881 | −0.11272 | 0.08183 | 5 to 140 barg |
| Pump, centrifugal | −0.3935 | 0.3957 | −0.00226 | 10 to 100 barg |

### Process vessels take a different route

Vessels do not use Eq. (A.3). They use the ASME thickness calculation directly, Turton Eq. (7.9) and (7.10), because the wall thickness of a pressure vessel is a design calculation with a code behind it as opposed to a fitted correlation:

$$t = \frac{P D}{2 S E - 1.2 P} + \mathrm{CA}$$

$$F_P = \begin{cases}
1.25 & P < -0.5\ \text{barg (vacuum)} \\
1 & t < t_{\min} \\
t / t_{\min} & \text{otherwise}
\end{cases}$$

with the constants Eq. (7.10) assumes throughout:

| Symbol | Value | Meaning |
|---|---|---|
| $S$ | 944 bar | Maximum allowable stress, carbon steel |
| $\mathrm{CA}$ | 0.00315 m | Corrosion allowance, 1/8 inch |
| $t_{\min}$ | 0.0063 m | Minimum wall thickness, 1/4 inch |

The weld efficiency $E$ has no default. Eq. (7.10) is printed with $E = 0.9$, but both vessels in Turton's own Example 7.14 back-solve to $E = 0.750$ from the example's printed pressure factors. Both values are exposed as named constants, the printed one and the one the worked example actually used, and the caller chooses. Recording that the book is internally inconsistent here is more useful than picking one value and hiding the other.

## Material factor

$F_M$ has no default and no table. Turton publishes it as Fig. A.18, a graph, so any number read from it is a reading instead of a citation. The module therefore requires $F_M$ as a caller input and exposes only the handful of values that Turton's own worked examples print:

| Combination | $F_M$ | Source |
|---|---|---|
| Shell and tube, CS shell and CS tube | 1.00 | Example 7.10 |
| Shell and tube, CS shell and SS tube | 1.81 | printed |
| Shell and tube, SS shell and SS tube | 2.73 | Example 7.12 |
| Vertical vessel, stainless steel | 3.11 | Example 7.13 |

Anything outside that list is the caller's to supply and to justify.

## Bare-module cost

Turton Eq. (A.4), for heat exchangers, process vessels and pumps:

$$F_{BM} = B_1 + B_2 F_M F_P$$

$$C_{BM} = C_p^0 \, F_{BM}$$

| Equipment | $B_1$ | $B_2$ |
|---|---|---|
| Exchanger: floating head, fixed tube, U-tube, kettle | 1.63 | 1.66 |
| Exchanger: double pipe, scraped wall, spiral | 1.74 | 1.55 |
| Process vessel, horizontal | 1.49 | 1.52 |
| Process vessel, vertical, including towers | 2.25 | 1.82 |
| Pump, centrifugal and reciprocating | 1.89 | 1.35 |

Worked example of the arithmetic: a floating-head exchanger in carbon steel at a pressure where $F_P = 1$ gives $F_{BM} = 1.63 + 1.66 \times 1 \times 1 =
3.29$. The purchased cost is multiplied by more than three to reach an installed cost, which is the point made in the background section.

### Equipment that skips the material and pressure step

Compressors, trays and packing are Table A.5 equipment. For these, Turton publishes $F_{BM}$ directly as a single lookup from Fig. A.19, with no separate $F_M$ and $F_P$ decomposition. The arithmetic is identical, one multiplication, but the conventions differ and the two paths are named separately in the code so that the difference cannot be lost.

| Item | $F_{BM}$ | Provenance |
|---|---|---|
| Compressor, carbon steel | 2.75 | digitised from Fig. A.19 at 400 dpi |
| Compressor, stainless steel | 5.7 | digitised from Fig. A.19 at 400 dpi |
| Compressor, nickel alloy | 11.5 | digitised from Fig. A.19 at 400 dpi |
| Sieve tray, stainless steel | 1.83 | printed, Example 7.14 |
| Sieve tray, carbon steel | 1.0185 | back-solved from Example 7.14's totals |

The three compressor values are flagged as digitised because a number read off a printed graph carries a real uncertainty that a number read off a table does not. The carbon steel tray factor is flagged as derived because it was recovered by inverting the example's own totals instead of read anywhere.

## From bare module to a plant

Turton Eq. (7.15) adds contingency and contractor fee:

$$C_{TM} = 1.18 \sum C_{BM}$$

Turton Eq. (7.16) adds the cost of site preparation and auxiliary facilities, which scale with the base-case equipment instead of with the upgraded materials and pressures:

$$C_{GR} = C_{TM} + 0.50 \sum C_{BM}^{\text{base}}$$

The base-case sum is the same equipment list re-evaluated at $F_M = F_P = 1$. This distinction matters and is easy to get wrong: buildings and roads do not become more expensive because the reactor is made of stainless steel, so the grassroots adder is not applied to the material-upgraded cost.

## Escalation

$$C(\text{year}) = C(2001) \times \frac{\mathrm{CEPCI}(\text{year})}{397}$$

The denominator is 397, and it deserves a note because there are two ways to read it. It is both the tabulated index for 2001 and the stated basis of the Table A.1 constants. Those agree here, so the distinction never bites in this project, but a reader escalating from a different correlation set should confirm which basis its constants were fitted on instead of assuming the tabulated index for the same year.

Table 7.4 is reproduced in full and covers 1996 to 2016:

| Year | Index | Year | Index | Year | Index |
|---|---|---|---|---|---|
| 1996 | 382.0 | 2003 | 402.0 | 2010 | 550.8 |
| 1997 | 386.5 | 2004 | 444.2 | 2011 | 585.7 |
| 1998 | 389.5 | 2005 | 468.2 | 2012 | 584.6 |
| 1999 | 390.6 | 2006 | 499.6 | 2013 | 567.3 |
| 2000 | 394.1 | 2007 | 525.4 | 2014 | 576.1 |
| 2001 | 397.0 | 2008 | 575.4 | 2015 | 556.8 |
| 2002 | 395.6 | 2009 | 521.9 | 2016 | 541.7 |

A year outside that range returns a lookup failure instead of an extrapolated number. Extrapolating a cost index is forecasting, and a costing module has no business doing it. A caller who wants a later year must supply the index, which makes that assumption theirs and visible.

## Where the size argument comes from

Eq. (A.1) needs a size. For a compressor the flowsheet supplies the shaft power directly, and for an exchanger it supplies the area. Vessels and columns need a geometry calculation, and two modules provide it.

### Vertical separator diameter

Turton Eq. (23.10) sizes a vertical vessel so the upward gas velocity stays below the terminal settling velocity of a 100 micron droplet:

$$D_{\text{ves}} = 15300 \sqrt{\frac{Q_g \mu_g}{\rho_L - \rho_g}}$$

with $Q_g$ in $\mathrm{m^{3}/s}$, $\mu_g$ in Pa·s and densities in $\mathrm{kg/m^{3}}$, returning metres. The constant 15300 folds together Stokes' law for the design droplet and the continuity relation $u_g = Q_g / (\pi D^2/4)$. It is not a unit conversion to be tidied up, and reproducing it exactly is how the module was checked.

Length comes from Table 23.10's length-to-diameter ratios instead of from a holdup-time calculation, which is the shortcut Turton's own text sanctions:

| Design pressure | Stated $L/D$ | Used here |
|---|---|---|
| Below 18 bar | 2 to 2.5 | 2.25 |
| 18 to 36 bar | 3.0 to 4.0 | 3.5 |
| Above 36 bar | 4.0 to 6.0 | 5.0 |

Collapsing a stated range to its midpoint is a simplification, and it is flagged as one. It is not an invented number: the range it collapses is published.

Only vertical vessels are implemented. Horizontal sizing needs an assumed liquid level and an iterative cross-sectional area calculation, and none of the source papers state an orientation for their knockout drums. Turton's own stated preference for horizontal applies when large liquid slugs may be present, which a post-reactor vapour and liquid split is not.

The gas viscosity this equation needs comes from the transport module, and carries that module's own flagged limitation. This does not introduce a new gap; it makes an existing one visible at the point where it affects a cost.

### Column diameter

Turton Sec. 21.3.2.2, the Fair and Matthews flooding correlation with constants curve-fitted by Wankat. Four steps.

The flow parameter, Eq. (21.59):

$$F_{lv} = \frac{L M_L}{V M_V} \sqrt{\frac{\rho_V}{\rho_L}}$$

$F_{lv}$ is dimensionless and depends on a flow ratio, so any consistent molar flow unit cancels. Turton's own Example 21.4 omits the molecular weights entirely, on the grounds that vapour and liquid compositions on a tray are similar. This module keeps $M_L$ and $M_V$ as separate optional inputs and defaults both to the vapour molar mass, which reproduces the same simplification without hiding it.

The capacity factor at flooding, Eq. (21.58):

$$\log_{10} C_{sb,\text{flood}} = -a - b \log_{10} F_{lv} - c \left(\log_{10} F_{lv}\right)^2$$

with one constant set per tray spacing, from Table 21.7:

| Tray spacing | $a$ | $b$ | $c$ |
|---|---|---|---|
| 6 in | 1.1977 | 0.53143 | 0.18760 |
| 9 in | 1.1622 | 0.56014 | 0.18168 |
| 12 in | 1.0674 | 0.55780 | 0.17919 |
| 18 in | 1.0262 | 0.63513 | 0.20097 |
| 24 in | 0.94506 | 0.70234 | 0.22618 |
| 36 in | 0.85984 | 0.73980 | 0.23735 |

The 24 inch row is the one Turton's own Example 21.4 uses, and the one this project defaults to.

The flooding velocity, Eq. (21.60), solved for $u_f$:

$$C_{sb,\text{flood}} = u_f \left(\frac{\sigma}{20}\right)^{0.2}
\sqrt{\frac{\rho_V}{\rho_L - \rho_V}}$$

$u_f$ is in ft/s, an artifact of the original correlation for which Turton states no metric equivalent exists. The surface tension defaults to 20 dyne/cm, which makes that term exactly unity. This is not an assumption of convenience: Turton's own example sets the term to unity and explains that the small exponent limits the error, worst case around 30 percent for pure water at room temperature and much less for organics.

Then the area and diameter, Eq. (21.61):

$$A_{\text{active}} = \frac{V M_V}{\rho_V \, u_{\text{actual}}}, \qquad
u_{\text{actual}} = f_{\text{flood}} \, u_f$$

$$A_{\text{actual}} = \frac{A_{\text{active}}}{f_{\text{active}}}, \qquad
D = \sqrt{\frac{4 A_{\text{actual}}}{\pi}}$$

with $f_{\text{flood}} = 0.75$ and $f_{\text{active}} = 0.88$, the exact values Example 21.4 uses, both inside Turton's own stated typical ranges of 0.75 to
0.80 and 0.85 to 0.90.

The column is sized at both the top and the bottom section and the larger diameter is taken, which is Turton's stated practice: a column is built to handle its limiting section.

## Modules

| File | Contents |
|---|---|
| `economics/capex_opex` | Eq. (A.1), (A.3), (A.4), (7.9), (7.10), (7.15), (7.16), CEPCI table, Table A.1, A.2, A.4 constants |
| `economics/vessel_sizing` | Eq. (23.10) and Table 23.10 |
| `economics/column_sizing` | Eq. (21.58) to (21.61), Table 21.7, and the bubble point that supplies real column conditions |
| `economics/separator_capex` | Composes the above into costed line items for vessels and tray stacks |

## Verification

| Check | Result |
|---|---|
| Table A.1, 9 rows, 45 constants | exact against the printed table |
| Table A.2, A.4 constants | exact |
| CEPCI Table 7.4, 21 years | exact, no extrapolation outside |
| Eq. (A.1) at $A = 1$ | $10^{K_1} = 67{,}702$ |
| Eq. (A.4) worked case | $F_{BM} = 3.29$ at $F_M = F_P = 1$ |
| Eq. (7.15), (7.16) | reproduce on a synthetic equipment list |
| Eq. (23.10) against Examples 23.2 and 23.3 | 4.218 m and 0.990 m, both exact |
| Eq. (21.58) to (21.61) against Example 21.4 | $F_{lv} = 0.145$, $C_{sb} = 0.30540$ against a printed 0.3054, $u_f = 3.025$ ft/s, area 3.727 $\mathrm{m^{2}}$ against a printed 3.73 |
| Vacuum branch of Eq. (7.10) | 1.25 |
| Example 7.14, all seven items end to end | \$797,116 against a printed \$797,000 |
| Example 7.14 escalated to 2016 | \$1,088,254 against a printed \$1,088,100 |

The Example 7.14 reproduction is the strongest single check in this chapter, because one number at the end depends on every step of the chain being right: Eq. (A.1) for seven purchased costs, Eq. (A.3) for a pressure factor, Fig. A.18 for two material factors, Eq. (A.4) for six bare-module factors, and the escalation. The agreement is inside Turton's own rounding, which is to the nearest hundred dollars.

The column and vessel equations both render as images in the source PDF and could not be extracted as text. They were read from 200 dpi page renders and then verified against the book's own worked examples, which is the check that matters: an equation transcribed correctly reproduces the printed answers from the printed inputs, and these do.