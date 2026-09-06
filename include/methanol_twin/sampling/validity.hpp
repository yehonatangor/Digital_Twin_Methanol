#pragma once
// Is this design point inside the range the correlations were fitted over?

// A correlation evaluated outside its window returns a finite, plausible number with no runtime signal
// Tolerable under human supervision, not under a sampler, and worst under an optimiser: 
// extrapolated regions are where a correlation is least constrained and a false optimum is most likely

// Reports only. Nothing is blocked and no result changes. 
// Each excursion carries the quantity, the model, the bound, the citation FOR THE BOUND, 
// and the overshoot as a fraction of the window width

// A clean report means "nothing known to be violated", NOT "verified"
// Bounds exist only where one has actually been read; kUnbounded skips a side

// See docs/25-model-provenance.md

#include <limits>
#include <string>
#include <vector>

#include "reactor_core.hpp"
#include "trm_reactor.hpp"
#include "electrolyzer.hpp"
#include "hybrid_plant_design_point.hpp"

namespace validity {

// "Not bounded, or no bound sourced yet". Skipped
inline constexpr double kUnbounded = std::numeric_limits<double>::infinity();

enum class Severity {
  // Outside the fitted window, but the model is a smooth correlation and the excursion is modest
  // The number is an extrapolation
  Extrapolation,
  // Far enough outside that the result should not be trusted at all, or a bound representing a hard limit
  OutOfRange
};

struct Excursion {
  const char* quantity = ""; // "reactor inlet pressure"
  const char* model    = ""; // "LHHW methanol kinetics"
  const char* source   = ""; // citation for the BOUND, not for the value
  double value = 0.0;
  double lo = 0.0;
  double hi = 0.0;
  double relative = 0.0; // how far outside, as a fraction of the window width
  Severity severity = Severity::Extrapolation;
};

struct Report {
  std::vector<Excursion> excursions;

  bool within_validated_range() const { return excursions.empty(); }
  int  count(Severity s) const;
  // Largest `relative` across all excursions; 0.0 if none
  // Useful as a single scalar feature or sample weight
  double worst_relative_excursion() const;
  std::string summary() const;

  void add(const Excursion& e);
  void merge(const Report& other);
};

// Records an excursion only if value falls outside [lo, hi]. relative is the
// overshoot divided by the window width
void check_range(Report& r, double value, double lo, double hi,
                 const char* quantity, const char* model, const char* source,
                 double extrapolation_limit = 0.25);

// Cited bounds. Each constant records a range this project has actually read in
// a source document. Anything not listed here is unbounded in the model or has no sourced window yet
// see the list at the bottom

namespace bounds {

// LHHW methanol synthesis. SOURCED: Van-Dal Sec. 2.3.2, the Mignard and
// Pritchard refit "expanded the application range of the model up to 75 bar"
// This loop runs at 78 bar, so Van-Dal themselves are ~4 % past their own quoted range; a sweep pushing pressure higher compounds it
inline constexpr double kLhhwPressureMaxBar = 75.0;
inline constexpr const char* kLhhwPressureSource =
    "Van-Dal & Bouallou (2013) Sec. 2.3.2 (Mignard & Pritchard 2008 refit)";

// LHHW temperature: NOT SOURCED. Vanden Bussche & Froment's range is not
// quoted in Van-Dal and the 1996 paper has not been read. Unbounded.
inline constexpr double kLhhwTemperatureMinK = -kUnbounded;
inline constexpr double kLhhwTemperatureMaxK =  kUnbounded;

// Xu & Froment (1989) reforming kinetics. SOURCED: Table 1 experimental
// conditions. The Aboosadi validation case runs at 1100 K and 20 bar, outside both axes
// Aboosadi extrapolates and this project follows them
inline constexpr double kXuFromentTemperatureMinK = 773.0;
inline constexpr double kXuFromentTemperatureMaxK = 848.0;
inline constexpr double kXuFromentPressureMinBar  = 3.0;
inline constexpr double kXuFromentPressureMaxBar  = 15.0;
inline constexpr const char* kXuFromentSource =
    "Xu & Froment (1989) AIChE J. 35(1) Table 1, steam-reforming conditions";

// Peng-Robinson kappa correlation switchover. Below/above 0.49 different
// published forms apply; eos.cpp implements both, this is informational
inline constexpr double kPrOmegaSwitch = 0.49;

// Fraction of the iteration cap above which a converged node is not portable
// MEASURED, R = 3.00 at 1 % purge uses 48 % of the budget on
inline constexpr double kLoopMarginalCapFraction = 0.40;

}

// Module-level checkers. Each is non-invasive: it inspects a config/result pair that has already been computed and reports

// LHHW pressure window plus the DIPPR-102 viscosity windows for every species present in the stream
Report check_reactor(const reactor::ReactorState& inlet,
                     const reactor::ReactorConfig& cfg,
                     const reactor::ReactorResult& res);

// Tri-reformer: Xu & Froment temperature/pressure window, plus viscosity
Report check_trm(const front_end::TrmReactorState& inlet,
                 const front_end::TrmReactorConfig& cfg,
                 const front_end::TrmReactorResult& res);

// Surfaces the Table A.1 fit window ElectrolyzerResult already computes
Report check_electrolyzer(const front_end::ElectrolyzerResult& res);

// Turton CAPCOST size and pressure windows. Costing outside Table A.1's stated range is a source of nonsense CAPEX
Report check_costing_size(double capacity, double min_size, double max_size,
                          const char* equipment_name);
Report check_costing_pressure(double p_barg, double p_min_barg, double p_max_barg,
                              const char* equipment_name);

// Runs every applicable checker above and merges. The one a sweep should call after solving
Report check_design_point(const flowsheet::HybridPlantDesignPointConfig& cfg,
                          const flowsheet::HybridPlantDesignPointResult& res);

// Recycle-loop feasibility, checked BEFORE solving. 
// Everything above inspects a result; a sweep also needs to know whether a point is worth solving
// A lookup over measured nodes, not a model: the feasible region is NOT convex 
// (at 1 % purge the loop closes at R = 2.70 and 2.85 but not 2.75 or 2.80), so any smooth rule would misreport
// See docs/25-model-provenance.md
enum class LoopFeasibility {
  Feasible, // converged using under kLoopMarginalCapFraction of the cap
  Marginal, // converged, but above it; not portable across platforms
  Infeasible, // tear iteration or bed integration failed
  Unmeasured // off-grid; nothing interpolated, solve and classify instead
};

const char* to_string(LoopFeasibility f);

struct LoopFeasibilityReport {
  LoopFeasibility feasibility = LoopFeasibility::Unmeasured;
  double nearest_ratio = 0.0; // the measured node consulted
  double nearest_purge = 0.0;
  int    iterations_measured = 0; // 0 when the node did not converge
  double cap_fraction = 0.0; // iterations / max_iterations at that node
  const char* note = "";
};

// Exact-node lookup within a small tolerance on both axes
LoopFeasibilityReport check_recycle_loop_before_solving(double fresh_h2_to_co2_ratio,
                                                        double purge_fraction);

// BOUNDS STILL NEEDED
//   - LHHW temperature window (needs Vanden Bussche & Froment 1996 directly)
//   - NRTL methanol/water temperature window (ChemSep gives no range)
//   - DIPPR-101 vapour-pressure windows (species.cpp stores no range)
//   - Fichtl deactivation kinetics: valid 523-553 K per activity_decay.hpp's
//     own preset naming, but the off-band 493 K preset is already used in
//     tests -- worth encoding once the intent is settled
//   - Trimm & Lam / De Smet combustion kinetics window
// Adding any of these is a one-line constant plus one check_range() call

}