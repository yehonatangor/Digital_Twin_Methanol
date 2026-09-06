# Model Provenance

The rule every constant obeys, the mechanism that reports when a correlation is used outside its fitted window, the labels that turn a failed evaluation into data, and the hash that ties a dataset to the model that produced it.

## Background: what a simulator owes a machine learning pipeline

A physical model read by a person and a physical model read by a sampler carry different obligations, and the difference is not about accuracy.

A person picking an operating point knows roughly where the source papers were working, notices when a result looks absurd, and reads the caveats in a header. None of that survives contact with an automated pipeline. A sampler generates points wherever its bounds allow, an optimiser actively seeks whatever looks best, and a surrogate learns whatever it is shown without asking where the numbers came from.

That changes what the simulator has to provide. Three things in particular.

A correlation evaluated outside its fitted range returns a finite, plausible-looking number with no runtime signal that anything is wrong. This is the central problem. It is tolerable under human supervision and not tolerable under automation, and it is worse for an optimiser than for a sampler. Extrapolated regions are exactly where a correlation is least constrained by evidence, which makes them exactly where a spuriously attractive optimum is most likely to appear. An optimiser will find them and drive straight in.

A failed evaluation is information, not missing data. Failures are not scattered randomly through a design space. They cluster at its edges and around real physical limits. Dropping them teaches a surrogate a smooth world with no walls in it, and an optimiser then walks through where a wall should be.

A dataset outlives the model that made it. Fix a constant six months after generating a training set and the surrogate now encodes physics the simulator no longer believes, with nothing anywhere recording the mismatch.

The three modules in this chapter address those three problems. None of them changes a result or blocks anything. They report.

## The sourcing rule

Every numeric constant in this project is exactly one of three things, and which one is stated at the point of use.

Sourced. Traceable to a specific table, equation or passage in a named publication. The citation is in the code, not only in a document.

Derived. Computed from sourced values by a derivation written out in the documentation, and flagged with a `_sourced = false` field. Derived is not sourced, and collapsing the two would hide the assumption the derivation rests on. The overall heat transfer coefficient is the main example, with its derivation in a document of its own.

Placeholder. Not available from any source in the project's library, marked as such in the code, and accompanied by a statement of what would close it. The catalyst price and the tri-reformer bed void fraction are examples.

There are no unlabelled numbers. The practical consequence is that a value which cannot be labelled cannot be added, and that constraint is what made the verification pass find no errors: most of what it would have caught was prevented from being written.

A few flags are worth naming because they are easy to overlook.

| Flag | Meaning |
|---|---|
| `U_sourced = false` | Heat transfer coefficient is derived, not read |
| `kCatalystPriceSourced = false` | No price exists in the library |
| `kCatalystReplacementIntervalSourced = false` | Engineering assumption, not from the decay law |
| `water_price_unit_sourced` | Records that a blank unit cell was resolved by the table's own convention |
| `p_max_from_cost_basis` | Storage ceiling inferred from a cost reference, not a stated operating limit |
| `permeate_pressure_assumed_at_feed` | Which of two membrane conventions produced a compression figure |
| `FP_sourced = false` | Vessel pressure factor computed rather than quoted |

## Validity ranges

Every correlation was regressed over a finite window. This module records the windows this project has actually read in a source, checks a design point against them, and reports excursions.

An excursion carries the quantity, the model whose window was left, the numeric bounds, the value, and the citation for the bound. The citation is for the bound, not for the value, which is a distinction worth keeping: knowing where a limit came from is what lets a reader judge whether crossing it matters.

The overshoot is normalised by the window width:

$$\text{relative} = \frac{\text{overshoot}}{\text{hi} - \text{lo}}$$

so 0.1 means the point sits a tenth of the fitted window's width beyond its edge. That gives a single scalar usable directly as a sample weight or a feature. Excursions are graded into two severities: an extrapolation, where the model is a smooth correlation and the overshoot is modest, and out of range, where either the excursion is large or the bound is a hard limit rather than a regression window.

### The encoded bounds

| Model | Quantity | Window | Source |
|---|---|---|---|
| LHHW methanol synthesis | Pressure | up to 75 bar | Van-Dal Sec. 2.3.2, Mignard and Pritchard refit |
| LHHW methanol synthesis | Temperature | not sourced, left unbounded | |
| Xu and Froment reforming | Temperature | 773 to 848 K | Xu and Froment Table 1 |
| Xu and Froment reforming | Pressure | 3.0 to 15.0 bar | Xu and Froment Table 1 |
| Turton Table A.1 | Equipment size | per equipment row | Table A.1 |
| Turton Table A.2 | Pressure | 5 to 140 barg for exchangers | Table A.2 |

Two of these fire in normal operation, and both are inherited from the sources instead of introduced here.

The synthesis loop runs at 78 bar against a 75 bar kinetic window. Van-Dal themselves operate about 4 percent past the range they quote for their own kinetics. That is a mild extrapolation inherited from the source, but a sweep that pushes pressure higher compounds it, which is exactly why it is reported instead of noted once in a header.

The tri-reforming case runs at 1100 K and 20 bar against a window of 773 to 848 K and 3 to 15 bar. Outside on both axes, and substantially so. Aboosadi extrapolates Xu and Froment, and this project follows them. The whole tri-reforming pathway should be read with that in mind, and the mechanism makes it visible in every row of a sampled dataset instead of in a paragraph somebody has to remember.

The LHHW temperature window is unbounded because Vanden Bussche and Froment's own experimental range is not quoted in Van-Dal and the 1996 paper has not been read directly. Leaving it unbounded is deliberate. Guessing a window would produce clean reports that mean nothing.

That points at the module's real limitation, which is stated in its own header and repeated here. A clean report means nothing known to be violated, not everything verified. Bounds exist only where one has actually been read. Absence of an excursion is not a certificate.

## Feasibility labels

Across this project a failed evaluation surfaces as an `ok = false` flag and a human-readable message. That is right for a person reading one result and useless for a dataset, where it becomes a dropped row or a NaN.

This module classifies the message into a stable enumeration instead. A feasible or infeasible flag trains a constraint classifier; the specific outcome trains a better one and tells you which part of the model a sampler is stressing.

| Group | Outcomes |
|---|---|
| Success | `Feasible` |
| Input | `InvalidInput` |
| Reactor | `ReactorTemperatureBound`, `ReactorPressureBound`, `ReactorRhsFailure`, `ReactorClampViolation`, `ReactorGeometryInvalid` |
| Separation and loop | `FlashFailure`, `RecycleNotConverged` |
| Downstream | `CompressionFailure`, `EconomicsFailure` |
| Fallback | `UnknownFailure` |

Two design decisions matter for anyone consuming the output.

The integer codes are frozen. New outcomes are appended to the end of the enumeration and existing ones are never renumbered, because renumbering changes the meaning of every dataset already generated. `Feasible` is zero and stays zero.

The original message is preserved verbatim. Classification is lossy, and keeping the source text means a row that lands in `UnknownFailure` can still be diagnosed instead of discarded.

The distinction the enumeration buys is real. A thermal runaway and a recycle loop that ran out of iterations are different constraints with different gradients in the design space. Collapsing both into a NaN throws that away, and the loop failure in particular marks a boundary that this project has already shown to be non-convex.

## The model fingerprint

A training set is only as meaningful as the simulator that produced it, and this one is under revision. Numerical upgrades to integration resolution or parameter values alter outputs, meaning the data provenance must track the exact configuration used.

The failure mode is quiet and expensive. A surrogate trained before a fix encodes physics the simulator no longer believes, and nothing records the mismatch. Months later nobody can say which model version any given dataset came from.

So every dataset carries a hash of the constants that generated it. Comparing two fingerprints answers whether they came from the same model without needing the source tree.

What is hashed is the physically meaningful constant set: formation enthalpies and entropies, heat capacity coefficients, critical properties, vapour pressure coefficients, LHHW parameters, Xu and Froment parameters, viscosity coefficients, Peng-Robinson binary interaction parameters, NRTL binaries, and the integrator's own accuracy settings. The last of those belongs in the hash because step size and clamp tolerance change results.

What is not hashed is anything a caller legitimately varies per design point: bed geometry, activity, feed rates, prices. Those are inputs, recorded per row. Hashing them would give every row a different fingerprint and defeat the purpose entirely.

Per-subsystem hashes are reported alongside the combined one, covering thermo, kinetics, transport, phase equilibrium and numerics. If two datasets differ, these localise the change without a diff of the source tree.

The stability contract is that the hash is FNV-1a over a canonical text serialisation, computed identically on every platform. No pointer values, no dependence on memory layout, and no floating point formatting ambiguity because values are serialised through their exact bit patterns. The same source tree gives the same fingerprint on Windows and Linux, at any optimisation level. That contract is what makes the hash usable as evidence instead of as a suggestion.

## Feasibility before solving, not only after

Everything above inspects a result. A sweep needs one thing that does not: a way to know whether a point is worth solving at all.

The recycle loop has a measured infeasible region, and it is not convex. At a 1 percent purge the loop closes at a fresh ratio of 2.70 and again at 2.85, but fails at 2.75 and 2.80. No interval or inequality describes that, so any smooth rule would report feasibility where none was measured.

`check_recycle_loop_before_solving` is therefore a lookup over the measured grid instead of a model, and it returns one of four things:

| Outcome | Meaning |
|---|---|
| `Feasible` | Measured, converged using under 40 percent of the iteration budget |
| `Marginal` | Measured, converged but above that threshold. Known not to converge on every platform |
| `Infeasible` | Measured, the tear iteration or the bed integration failed |
| `Unmeasured` | Not on the grid. Nothing is interpolated; solve it and classify the result |

The `Marginal` case exists because of a real portability failure. At the stoichiometric feed ratio and a 1 percent purge the loop converges here in 291 of 600 passes, and settles into a non-converging limit cycle at a residual near $3 \times 10^{-3}$ under a different C library. The physics is identical; only the arithmetic differs. Two grid nodes carry that label, and the same threshold marks them in `docs/figures/03-operating-envelope.csv` through its `portable` column and with a dashed ring in the figure.

Reporting this instead of smoothing it over is the point. A surrogate trained on a dataset that includes a node its own generator cannot reproduce would encode arithmetic, not chemistry.

## Using them together

The three modules are meant to be read as three columns appended to every row of a sampled dataset:

| Column | From | Use |
|---|---|---|
| `loop_feasibility` | validity, before solving | Skip or down-weight before spending a solve |
| `within_validated_range` | validity | Filter or flag extrapolated rows |
| `worst_relative_excursion` | validity | Sample weight or feature |
| `outcome_code` | feasibility | Constraint classifier target |
| `model_hash` | fingerprint | Dataset provenance |

None of them changes a result. A row outside a fitted window is still computed and still returned; it is labelled. That is deliberate: filtering is the consumer's decision, and a module that dropped points would hide the shape of its own limits.

## Modules

| File | Contents |
|---|---|
| `sampling/validity` | Cited bounds, range checking, excursion severity, and the pre-solve loop feasibility lookup |
| `sampling/feasibility` | Outcome enumeration, frozen codes, message-preserving classification |
| `sampling/fingerprint` | FNV-1a over the canonical constant serialisation, per-subsystem hashes |

## Verification

| Check | Result |
|---|---|
| A value inside a window records nothing | asserted |
| Relative excursion equals overshoot over window width | 0.111111 on a worked case |
| Modest overshoot classifies as extrapolation, large as out of range | asserted |
| Undershoot caught as well as overshoot | asserted |
| A fully unbounded check never fires | asserted |
| A one-sided bound still fires | asserted |
| The 78 bar loop is flagged against the 75 bar LHHW window | asserted |
| A 50 bar run inside the window reports nothing | asserted |
| The Aboosadi case is flagged on both temperature and pressure | asserted |
| Turton size and pressure windows fire outside their ranges | asserted |
| A converged run classifies feasible, code 0 | asserted |
| A temperature trip classifies correctly and keeps its message verbatim | asserted |
| Outcome tokens contain no comma or whitespace | asserted |
| The fingerprint is deterministic | asserted |
| The design point is measured feasible | asserted |
| The stoichiometric node is flagged marginal | asserted |
| A measured non-converging node is infeasible | asserted |
| R 2.70 is feasible at 3 % and 1 % but not 2 %, so no interval describes it | asserted |
| An off-grid point returns unmeasured instead of a guess | asserted |

The token format test looks trivial and is not. These strings are written into CSV files, and a comma inside a categorical value corrupts every column after it.