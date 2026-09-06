# Validation

Every value reproduced from a published source, how each was checked, what was checked against an independent origin, what could not be, and the four places where a source contradicts itself.

## Background: what validation can and cannot establish

A simulation can be wrong in three separate ways, and they need different checks.

Transcription. A constant was copied incorrectly from a table. This is the most common error and the easiest to find, because it is checked by reading the source again. It is also the easiest to miss, because reading the same extracted text twice is not an independent check: a mangled character is mangled identically both times.

Implementation. The constants are right but the equation using them is assembled incorrectly. A sign, an exponent, a term in the wrong denominator. This is invisible to transcription checking and is caught by reproducing a worked example end to end, where the inputs and the printed answer both come from the source.

Applicability. Everything is correct and is being used outside the range it was fitted over. Nothing about the arithmetic reveals this, which is why it is handled by a separate mechanism, described in the next chapter.

The distinction matters for reading this chapter. A row saying a constant matches its table establishes only the first kind of correctness. Rows that reproduce a published worked example establish the second, and those are the stronger evidence.

## Method: three independent extraction layers

Saying that something was checked is worthless without saying how, and the layers found different things.

| Layer | Technique | Covers |
|---|---|---|
| 1 | PDF text layer extraction | Born-digital papers: Mucci, Lim, Shi, Van-Dal, Fichtl, Aboosadi |
| 2 | 400 dpi page render into OCR | Anything the text layer mangled or lacked; a genuinely different path, not a re-read |
| 3 | Independent databank and cross-source triangulation | Values with no directly readable source, checked against a different origin |

Layer 2 exists because it is a different extraction path, not a repetition. Several tables in this project's sources render as images, and equations frequently do. Turton Eq. (7.9), (7.10), (21.58) to (21.61) and (23.10) all render as images and could not be extracted as text at all.

Layer 3 is the only one that can catch an error present in the source itself.

## Overall result

| | |
|---|---|
| Values checked against a primary source | about 120 |
| Discrepancies found in the code | 0 |
| Discrepancies found in the sources | 4 |
| Items unverifiable from the PDFs | Turton Tables A.2 and A.4 only |

## Reproduced worked examples

These are the strongest checks, because inputs and answers both come from the source and the whole equation chain has to be right to land on the printed number.

| Source | Example | Checked | Result |
|---|---|---|---|
| Turton | Example 21.4 | $F_{lv}$ | 0.145 exactly |
| Turton | Example 21.4 | $C_{sb,\text{flood}}$ | 0.30540 against a printed 0.3054 |
| Turton | Example 21.4 | Flooding velocity | 3.025 ft/s |
| Turton | Example 21.4 | Tower area | 3.727 $\mathrm{m^{2}}$ against a printed 3.73 |
| Turton | Example 23.2 | Vessel diameter | 4.218 m exactly |
| Turton | Example 23.3 | Vessel diameter | 0.990 m exactly |
| Turton | Example 7.14 | Seven items end to end, purchased and bare-module cost | total \$797,116 against a printed \$797,000 |
| Turton | Example 7.14 | CEPCI escalation to 2016 | \$1,088,254 against a printed \$1,088,100 |
| Turton | Example 7.14 | Vessel weld efficiency | both vessels back-solve to $E = 0.750$ |
| Mucci | Appendix A.3 | Storage vessel cost | 7834.7 $\mathrm{USD/m^{3}}$ against a stated "around 7800" |
| Van-Dal | Sec. 2.3.1 | Per-pass conversion | 33.84 % against a stated 33 % |
| Fichtl | Table 4 and text | Activity loss over 1630 h at 523 K | 58.2 / 56.8 / 38.1 % against a stated 60 / 60 / 40 % |
| Aboosadi | Table 8 | Tri-reformer outlet | within 3.4 % on methane conversion |

## Constant tables verified

| Source | Table | Scope | Result |
|---|---|---|---|
| Turton | A.1 | 9 rows, 45 constants | exact |
| Turton | 7.4 | CEPCI, 21 years | exact |
| Turton | 21.7 | Fair and Wankat flooding constants, 6 rows | exact, and reproduces Example 21.4 |
| Turton | 23.10 | Length to diameter ratios | exact, midpoints taken and flagged |
| Lim | 1 | 7 prices and rates | exact |
| Lim | 2 | Electrolyser capital, 3 technologies, 4 years each | exact |
| Mucci | 2 and A.1 | PEM efficiency polynomial, 4 coefficients | exact |
| Mucci | 3.4 and A.2 | Storage density slope, pressures, update factor | exact |
| Fichtl | 4 | Decay constants and orders, 3 catalysts, 3 temperatures | exact |
| Xu and Froment | 5 and 6 | 7 pre-exponentials, 7 activation and adsorption enthalpies | exact |
| Aboosadi | 2 and 3 | Rate constants, van't Hoff pairs, effectiveness factors | exact |
| Shi | 3.1 | Tube geometry, recycle fraction, separator temperature, column specification | exact |
| Van-Dal | 2 and A.3 | Catalyst properties, charge, operating conditions, lab bed | exact |

## Independent cross-checks

Where a value could be confirmed from a second, unrelated origin, it was. These are the checks that could have caught an error in the source.

Xu and Froment against Aboosadi. Aboosadi's published rate constants equal Xu and Froment's divided by exactly 3.6, which is the kmol/h to mol/s conversion. Two papers, one relationship, and it confirms both the transcription and the unit handling at once.

Xu and Froment's own confidence intervals. The 400 dpi render recovered the paper's published upper and lower limits, which the text layer had not produced. Every reference value used as a test target falls inside the published interval:

| Constant | Lower | Test target | Upper |
|---|---|---|---|
| $k_{1,648}$ | 1.64e-4 | 1.842e-4 | 2.05e-4 |
| $k_{2,648}$ | 6.915 | 7.558 | 8.200 |
| $k_{3,648}$ | 1.78e-5 | 2.193e-5 | 2.60e-5 |
| $K_{CO,648}$ | 37.17 | 40.91 | 44.65 |
| $K_{H_2,648}$ | 0.0059 | 0.0296 | 0.0533 |
| $K_{CH_4,823}$ | 0.1371 | 0.1791 | 0.2211 |
| $K_{H_2O,823}$ | 0.0317 | 0.4152 | 0.5032 |

This closed a real gap. Before it, those test targets were only checkable against themselves.

The hydrogen adsorption sign. OCR rendered it ambiguously in Xu and Froment. It is pinned independently by Aboosadi Table 3.

Thermodynamic reference data. Formation enthalpies and entropies, heat capacity correlations and vapour pressure correlations were checked against independent thermophysical databanks rather than against the process papers that use them.

Shi's bulk density. The paper prints 1140 $\mathrm{kg/m^{3}}$, and the code derives it as $1900 \times 0.60$ from the particle density and void fraction. Both routes agree, which confirms the derivation rather than merely the number.

## Four source-side inconsistencies

In the first three the code had already made the defensible choice. The fourth, the industrial loop tension described below, remains open.

### Van-Dal Eq. (9), inverted signs

Equation 9 is printed as

$$\log_{10} K_{eq,2} = \frac{-2073}{T} + 2.029$$

with both signs opposite to Graaf's original, which the same paper reproduces intact one equation earlier. As printed, the constant would rise with temperature, making the reverse water gas shift endothermic in the wrong direction and giving a value around 0.007 at synthesis conditions against a literature range of 130 to 150.

The implemented form is Graaf's:

$$\log_{10} K_{eq,2} = \frac{2073}{T} - 2.029$$

which gives 149.48 at 493.15 K, inside the literature range, falling with temperature as an exothermic equilibrium must. A sign convention that reverses between adjacent equations drawn from the same source is a typesetting error, not a modelling choice, and the test suite asserts both the value and its temperature trend so a future edit cannot reintroduce the printed form.

### Van-Dal's over-determined laboratory bed

The paper prints all five of a 0.016 m diameter, 0.15 m length, 34.8 g charge, a void fraction of 0.5 and a particle density of 1775 $\mathrm{kg/m^{3}}$. Those five are mutually inconsistent: any four determine the fifth, and the printed fifth does not match.

There is no way to choose correctly from the paper alone, so the code does not choose. Two presets are exposed, one taking the mass as primary and one taking the length as primary, and the caller picks. Making the ambiguity a visible input is more honest than resolving it in either direction.

### Fichtl's aging temperature

The body text says a third order fit applies at 523 and 553 K and a fourth order fit at 483 K, while Table 4's column header reads 493 K. The code follows Table 4, on the reasoning that the table is the primary data and the body text is prose about it.

## What was checked at the plant level

Beyond individual constants, four whole-flowsheet behaviours were checked against published statements.

| Claim | Source | Model |
|---|---|---|
| Per-pass conversion, laboratory case | Van-Dal, 33 % | 33.84 % |
| Catalyst charge | Van-Dal, 44,500 kg | used unscaled |
| Purge fraction | Van-Dal, 1 % | used as the default |
| Recycle fraction | Shi, 98 % | reproduced when configured |

### The industrial loop, and a fourth source-side tension

Van-Dal states four things about the industrial loop, and they are worth listing because this project can now check all four at once on the published charge with no free parameter. The purge is sourced directly: *"Some of the non-reacted gases (1%) are purged to minimise the accumulation of inerts and by-products in the reaction loop."* Alongside it the paper states a recycle ratio of 5.0, a per-pass conversion of 33 percent, and an overall conversion of about 93 percent.

Holding the purge at the published 1 percent and sweeping only the fresh feed ratio:

| $R$ | Per-pass | Overall | Recycle / fresh | Circulator |
|---|---|---|---|---|
| 2.70 | 13.2 % | 90.1 % | 4.50 | 3.12 MW |
| 2.85 | 23.1 % | 94.9 % | 4.84 | 2.55 MW |
| 2.90 | 29.2 % | 96.4 % | 5.55 | 3.00 MW |
| 2.95 | 38.4 % | 97.8 % | 7.26 | 4.83 MW |
| Van-Dal | 33 % | ~93 % | 5.0 | |

The three published figures do not land at one feed ratio. The recycle ratio points to $R \approx 2.86$, the per-pass conversion to $R \approx 2.92$, and the overall conversion to something below 2.85. They bracket a narrow band, roughly
2.83 to 2.92, but no single operating point in this model reproduces all three simultaneously.

That is a fourth source-side tension to add to the three above, and it is the most informative of them, because the numbers are close enough that the model is clearly capturing the right loop and far enough apart that something is unresolved. Three explanations remain open. Van-Dal's recycle ratio may be defined on a different basis than the reactor-inlet to fresh-feed molar ratio used here. Their per-pass figure is quoted in the same sentence as the recycle ratio but may be carried over from the laboratory measurement at 50 bar and 220 $^\circ\mathrm{C}$ instead of recomputed for the industrial loop. Or the substituted tube geometry, which comes from Shi because Van-Dal publishes none, shifts the pressure drop enough to move the balance between per-pass conversion and recycle flow.

What the comparison does establish is stronger than what this document previously claimed. At Van-Dal's own published purge on Van-Dal's own published charge, with the feed ratio the only adjustable quantity, the model lands within two points of their overall conversion and within three percent of their recycle ratio. The purge is not a free parameter, because the paper states it.

Solving an adiabatic bed cannot close the loop below a 10 percent purge at the stoichiometric feed ratio. Changing the thermal boundary condition to the jacketed reactor removes this limitation entirely.

The per-pass figures are not comparable between the laboratory and plant cases and the laboratory row above is reported separately for that reason. Van-Dal's 33 percent is measured at 50 bar and 220 $^\circ\mathrm{C}$ on 34.8 g of catalyst; the plant runs at 78 bar on a recycle-diluted feed. Whether their industrial per-pass figure is a genuine second measurement or the same number restated is precisely what the tension above cannot resolve.

## What is not verified

Honest completeness requires naming what this chapter does not cover.

Values with no source in the library. The catalyst price, the catalyst replacement interval, the tri-reformer bed void fraction and particle density, and the material factors outside the four Turton worked examples print. All are flagged in code, and the next chapter describes the mechanism.

Derived instead of sourced values. The overall heat transfer coefficient is derived under a stated assumption with the derivation written out, and carries a flag saying so. Derived is not sourced.

Digitised values. The three compressor bare-module factors were read from a printed graph at 400 dpi. A number read off a graph carries a real uncertainty that a number read off a table does not, and they are flagged separately for that reason.

The dispatch results. The price-threshold rule is a heuristic, not a reproduction of any published optimisation, and its outputs are not compared against any published cost or flexibility figure.

Anything downstream of an unsourced input. A cost that includes the catalyst charge inherits the catalyst price flag. The validity mechanism in the next chapter propagates this instead of leaving it to a reader to trace.

## Where the reference values live, and which are independent

Every reference value the suite compares against sits in `tests/reference_data.hpp`, with its citation, and carries one of two labels. The labels exist because the difference decides what a passing test proves.

`INDEPENDENT` means the reference came from a source that did not supply the correlation under test, so agreement is evidence the correlation is right:

| Reference | Checks | Source |
|---|---|---|
| Methanol and water bubble curve, 11 points | NRTL activity coefficients and DIPPR-101 vapour pressure together | Gmehling and Onken, DECHEMA Vol. I |
| Reaction enthalpies at 298.15 K, 5 reactions | the formation table, Kirchhoff integration and stoichiometry together | Lim (2022), Shi (2020), Van-Dal (2013) |
| Normal boiling points, 2 species | DIPPR-101 must return 101.325 kPa there | CRC Handbook |
| Vapour viscosity, 9 species | DIPPR-102 with Perry's Table 2-312 coefficients | CRC and NIST |

`TRANSCRIPTION` means the code's own table and the reference share a source, so agreement shows only that the digits were copied correctly. The critical constants and the formation enthalpies and entropies are of this kind: `species.cpp` and `thermo.cpp` cite the same CRC and NIST-JANAF data the reference holds. Worth having, and not validation.

One gap is stated instead of papered over. The ideal-gas $C_p$ check is not automated. `species.cpp` takes its DIPPR-107 coefficients from Perry's 8th ed. Table 2-155, and the comparison against NIST WebBook gas-phase values was performed by hand across nine species and three temperatures, 298, 800 and 1300 K. Its result is recorded in `docs/03-species-properties.md`: worst deviation 1.74 percent, methane at 1300 K. What was not kept is the NIST absolute values, only the per-cell deviations, so there is nothing in the repository to assert against and the suite cannot reproduce it. Reconstructing the references from the code's own output plus the recorded deviations would be circular, so it has not been done.

What the suite does assert about $C_p$ is only what holds whatever the coefficients are: $C_p > 0$ for all nine species, the rise with temperature for a polyatomic, and a temperature-independent $C_p$ for monatomic argon. Closing the gap needs the NIST values themselves, after which it is one constant table and one comparison loop, exactly as was done for viscosity above.

Two corrections are on the record here. An earlier README claimed the $C_p$ comparison as a test-suite result, which it was not, and pointed at `tests/reference_data.hpp` for its data at a time when that file did not exist. The viscosity comparison had the opposite problem: `docs/10` held nine genuinely independent CRC and NIST reference values that no test asserted. Those are now in `reference_data.hpp` and checked.

## Build and test state

| Metric | Result |
|---|---|
| Translation units | 48 |
| Compiler warnings under `-Wall -Wextra` | 0 |
| Test binaries | 38 |
| Assertions | 1004 |
| Failures | 0 |

### One platform artefact, recorded because it looks like a defect

Running a heavy recycle binary directly from a shell, instead of through `ctest`, can raise `SIGFPE` on some Linux containers. It is not a defect in the model, and the evidence is short enough to state here so that anyone who meets it can confirm the diagnosis instead of re-derive it.

A `SIGFPE` handler installed by preloading a small shared library reads the floating-point control state out of the signal's own `ucontext`, which is the state in force at the faulting instruction:

| Quantity | Value |
|---|---|
| Faulting instruction | `call cos@plt`, in the inlined cubic solver of `src/eos.cpp` |
| `si_code` | 7, `FPE_FLTINV` |
| `MXCSR` at process start | `0x1f80`, every exception masked |
| `MXCSR` at the fault | `0x803f`, all six SSE masks cleared, all six flags set |
| x87 control word at the fault | `0x037f`, unchanged |

`0x803f` is the finding. Bits 7 to 12 of `MXCSR` are the SSE exception masks, and in that value every one is clear, so invalid is unmasked. Nothing in this project writes `MXCSR`; it is set to `0x1f80` by the loader and never touched. Once the masks are clear, the next SSE instruction that raises invalid delivers `#XF`, which the kernel reports as `SIGFPE`. The x87 control word is intact, which is why `fegetexcept()` reports 0: on glibc for x86-64 it reads the x87 word instead of `MXCSR`.

The argument to `cos` cannot be the cause. It is $(\varphi + 2\pi k)/3$ with $\varphi = \arccos t$ and $t$ clamped into $[-1, 1]$, so it lies in $[0, 5\pi/3]$ and is finite by construction, and a finite argument cannot raise the invalid flag. A standalone program compiled in the same container confirms that $\sqrt{-1}$, $\cos(\mathrm{NaN})$, $\arccos 2$ and $0/0$ all return NaN with no signal while the masks are intact.

The fault is layout-dependent instead of value-dependent, which settles it. The identical binary on the identical input passes under `ctest`, passes with an empty environment, and passes when any additional shared library is preloaded. A defect in the equation of state could not be sensitive to the size of the process environment block. The conclusion is that the container corrupts `MXCSR`, most likely in its floating-point state save and restore.

A separate report of a non-converging limit cycle on GCC with the UCRT runtime, at residual $2.98 \times 10^{-3}$ after the 600-iteration cap, is a different symptom with a real cause. It occurs at the $R = 3.00$, 1 percent purge node, which consumes 48 percent of the iteration budget here, more than double any other converging node. That node is labelled `MARGINAL` in `docs/figures/03-operating-envelope.csv`, drawn with a dashed ring in figure 03, and classified `Marginal` by `validity::check_recycle_loop_before_solving` before a sweep spends time on it. It is not the canonical design point, which is $R = 2.95$ at a 1 percent purge and converges in 115 passes.

## Bottom line

About 120 constants were checked against their primary sources and no errors were found in the code. The four inconsistencies found are in the papers. In three of them the code had already made the correct call and documented it; the fourth, Van-Dal's mutually inconsistent industrial loop figures, is reported above as an open question instead of resolved in either direction.

That is an unusual outcome for a model this size, and it is worth being clear about why it happened instead of treating it as luck. Every constant in this project is labelled sourced, derived or placeholder at the point of use, and a value that cannot be labelled cannot be added. The discipline is what made the verification pass find nothing, because most of the errors it would have found were prevented from being written.