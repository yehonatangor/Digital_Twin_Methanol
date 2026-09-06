# Plant Economics

Aggregating equipment costs into a plant capital cost, annualising it, adding operating cost, and dividing by production to get a cost per tonne. The chapter also states plainly which equipment categories are and are not in the total.

## Background: annualisation and the cost of money

Capital is spent once and the plant produces for decades, so the two cannot be added directly. The standard reconciliation is to convert the lump sum into an equivalent uniform annual payment, exactly as a mortgage converts a house price into a monthly payment. The conversion factor is the capital recovery factor:

$$\mathrm{CRF} = \frac{i(1+i)^n}{(1+i)^n - 1}$$

for interest rate $i$ over $n$ years. Two limits make the shape clear. As $i \to 0$ the factor tends to $1/n$, a simple division of the capital over the life with no cost of money. As $n \to \infty$ it tends to $i$, the perpetual interest payment on a loan never repaid. Real projects sit between: at 4.5 percent over 20 years the factor is 0.0769, so roughly 7.7 percent of the capital is charged to every year of operation, against 5 percent if money were free. The 2.7 point difference is the cost of capital and it is often larger than any single operating line.

The resulting figure of merit is a levelised cost:

$$C_{\text{unit}} = \frac{\mathrm{CRF} \times \mathrm{CAPEX} + \mathrm{OPEX}_{\text{annual}}}{\text{annual production}}$$

This is the number that answers whether a plant can sell at a given price, and it is the only economic output this project claims.

## Sources

Annualisation, prices and the operating cost structure follow Lim et al.,
*Renewable and Sustainable Energy Reviews* 155 (2022) 111876, Eq. (5) and (6), Table 1 and Table 2, Sec. 2.2.1. The capital cost primitives come from Turton and are described in the previous chapter.

## The scope boundary, stated first

This is a deliberately partial plant cost, and reading the total as a whole plant CAPEX would be a mistake. Turton's method needs a genuine physical size attribute for each item. Auditing every process module for what size data it actually publishes gives three categories.

Costed, with real sizes taken from an actual process run.

| Item | Size attribute | Route |
|---|---|---|
| Compressor | shaft power, MW, from the compressor result | Table A.1 fluid-power correlation, Fig. A.19 factor |
| Packed-bed reactor | heat transfer area, pure geometry | costed as a shell-and-tube exchanger |
| Electrolyser | total power, MW | all-in installed dollars per kW, Lim Table 2 |

The reactor's area is $n_{\text{tubes}} \pi D_{\text{tube}} L_{\text{bed}}$, which needs no heat transfer coefficient. A coefficient is only needed to convert a duty into an area, and this route never computes a duty.

Not costed, gap documented rather than worked around.

Generic two-stream heat exchangers cannot be costed because none of the four core papers publish a $UA$, let alone the separate $U$ needed to turn a duty into an area through $A = UA/U$. Inventing a $U$ to unblock a larger and more impressive CAPEX number would create exactly the kind of unsourced free parameter this project's discipline exists to prevent. A caller who has a real exchanger design can cost it through the Turton primitives directly.

The membrane is not costed either. Mucci costs it through a model published in a paper outside this project's library, and Turton has no membrane category at all.

Knockout drums and the distillation column were originally in this list. They have since been closed by the vessel and column sizing modules described in the previous chapter, which supply real geometry from real flows.

The result object carries a message stating which categories were included, so the boundary is visible at runtime and not only in this document.

## Capital aggregation

Line items accumulate into two separate pools, and the reason is a bug worth recording.

Turton-pipeline items carry a purchased cost and a bare-module factor. They are summed, marked up by Eq. (7.15), and escalated from the 2001 basis to the target year.

All-in installed items carry a cost that is already installed, already in its own currency year, and already includes contingency. Lim's Table 2 electrolyser figures in dollars per kilowatt are the case this exists for. Running such a number through the Turton pipeline applies two markups it already contains: the
1.18 contingency and fee, and the CEPCI escalation from 2001. At a 2016 target that is an inflation of

$$1.18 \times \frac{541.7}{397} = 1.61$$

on a line that is frequently the largest in the whole estimate. The hybrid flowsheet explicitly bypasses aggregation when sizing stack replacement to ensure the capital cost is not artificially inflated.

The fix is structural rather than a patch. The `all_in_installed` flag routes an item into a separate accumulator, which is added to the total once at face value. Both components are reported, so the split can be audited instead of inferred:

$$\mathrm{CAPEX}_{\text{total}} =
\underbrace{1.18 \left(\sum C_{BM}\right) \frac{\mathrm{CEPCI}_{\text{target}}}{397}}_{\text{Turton pipeline}}
+ \underbrace{\sum C_{\text{all-in}}}_{\text{face value}}$$

The base-case sum at $F_M = F_P = 1$ is carried alongside for the grassroots formula, as described in the previous chapter.

## Operating cost

Four lines, all computed from real flows taken from a converged process run instead of from assumed capacities.

Reactants. Mass flows in kilograms per second are converted to annual tonnes through the stream factor, then priced from Lim Table 1:

| Item | Price | Unit |
|---|---|---|
| Natural gas | 2.99 | USD per GJ |
| Carbon dioxide | 86.4 | USD per t |
| Water | 0.15 | USD per t |
| Hydrogen | 3.2 | USD per kg |
| Electricity | 0.06 | USD per kWh |
| Interest rate | 0.045 | |
| Stream factor | 0.9 | grid electricity case |

Water's row in the printed table has a blank unit cell. Table 1's own convention is to print a unit only where it changes, so the blank inherits USD per tonne from the carbon dioxide row above it. That resolution is recorded in a flag instead of assumed, because reading it as USD per kilogram would be a factor of a thousand.

Natural gas is priced per gigajoule, so its mass flow is converted through a lower heating value. That value is not taken from a handbook. It is derived from the project's own formation enthalpy table by the same route hydrogen's is:

$$\mathrm{CH_4} + 2\,\mathrm{O_2} \rightarrow \mathrm{CO_2} + 2\,\mathrm{H_2O(g)},
\qquad \mathrm{LHV} = \frac{-\Delta H(298.15\ \mathrm{K})}{M_{\mathrm{CH_4}}}$$

giving 50.03 MJ/kg against a handbook 50.0. Deriving it keeps a single source of truth: if the formation enthalpy table is ever corrected, every consumer of it moves together.

Electricity. Total electrical power multiplied by the stream factor and the price per kilowatt hour.

Maintenance. Lim Sec. 2.2.1 sets this at 20 percent of the reforming and methanol synthesis capital, excluding the electrolyser, whose degradation is handled separately by stack replacement. The caller passes only that subset of the capital as the base. Passing the whole plant capital would charge maintenance twice on the electrolyser.

Electrolyser stack replacement. Lim charges 30 percent of the initial electrolyser investment per replacement, with

$$n_{\text{replacements}} = \left\lfloor \frac{n_{\text{project}}}{\text{stack life}} \right\rfloor$$

Full cycles only. The source does not specify prorating a partial final cycle, so none is applied, and the annualisation is a simple division over the project life instead of a discounted one. Both simplifications are stated instead of smoothed over. At a 20 year project and an 8 year stack life that gives two replacements, not two and a half.

## Catalyst replacement

The synthesis catalyst is replaced on its own cycle, using the same discrete bookkeeping as the electrolyser stack but a different cost basis: mass times price per kilogram per replacement, instead of a fraction of an unrelated initial investment.

Two numbers here are not sourced and both are flagged.

Catalyst price. No paper in this project's library publishes a $\mathrm{Cu/ZnO/Al_2O_3}$ price. The placeholder is 20 USD per kg, a round number carrying `kCatalystPriceSourced = false`, deliberately visible instead of dressed as data.

Replacement interval. Three years, and this is worth explaining because a plausible-looking wrong answer is available. The deactivation correlation used elsewhere in this project is fitted to data topping out around 1600 hours, roughly 0.18 years. Reusing that as a replacement interval would imply changing the charge every two months, which contradicts industrial practice of a two to four year charge life and is not what the underlying data was measured to predict. The fitted range of a decay law and the economic life of a charge are different quantities, and conflating them would be a category error. Three years is a flagged engineering assumption reflecting typical practice, and it is caller-overridable.

## Putting it together

$$\mathrm{CRF} = \frac{i(1+i)^n}{(1+i)^n - 1}, \qquad
C_{\text{unit}} = \frac{\mathrm{CRF} \cdot \mathrm{CAPEX} + \mathrm{OPEX}}{\dot m_{\text{annual}}}$$

A worked case, pinned in the test suite. With a capital cost of 100 million dollars at 4.5 percent over 20 years, the capital recovery factor is 0.0768761 and the annualised capital is 7.688 million per year. Adding an operating cost of 66.704 million gives a total annual cost of 74.392 million. Against 250,000 tonnes per year of methanol that is 297.57 dollars per tonne.

## Modules

| File | Contents |
|---|---|
| `economics/plant_economics` | Line-item aggregation, the all-in installed exemption, OPEX, the top-level unit cost |
| `economics/capex_opex` | Capital recovery factor, price table, stack replacement, the six-tenths cross-check |
| `economics/catalyst_replacement` | Charge cost on its own replacement cycle |

## Verification

| Check | Result |
|---|---|
| Capital recovery factor at 4.5 percent over 20 years | 0.0768761 |
| Zero interest reduces to $1/n$ | 0.05 at 20 years |
| Lim Table 1, all seven values | exact |
| Water's unit resolved from the table's inheritance convention | recorded in a flag |
| Natural gas LHV derived from formation enthalpies | 50.03 MJ/kg against a handbook 50.0 |
| Stack replacements over 20 years at 8 year life | 2, not 2.5 |
| All-in installed item bypasses the 1.61 inflation | asserted directly |
| Maintenance base excludes the electrolyser | asserted |
| Worked unit cost | 297.57 USD/t |

The all-in installed test is the one worth keeping. It does not merely check a number, it asserts that the electrolyser line is not multiplied by 1.18 and not escalated, which is the specific error that was found and fixed.