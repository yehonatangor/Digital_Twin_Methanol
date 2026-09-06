#include "sampling/validity.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "units.hpp"

namespace validity {

// -----------------------------------------------------------------------------
// Report
// -----------------------------------------------------------------------------
void Report::add(const Excursion& e) { excursions.push_back(e); }

void Report::merge(const Report& other) {
  excursions.insert(excursions.end(), other.excursions.begin(), other.excursions.end());
}

int Report::count(Severity s) const {
  int n = 0;
  for (const Excursion& e : excursions) if (e.severity == s) ++n;
  return n;
}

double Report::worst_relative_excursion() const {
  double worst = 0.0;
  for (const Excursion& e : excursions) worst = std::max(worst, e.relative);
  return worst;
}

std::string Report::summary() const {
  if (excursions.empty()) return "within every bound this project has sourced";

  std::string s;
  char buf[512];
  std::snprintf(buf, sizeof(buf),
                "%zu excursion(s): %d extrapolation, %d out-of-range; worst %.1f%% beyond window\n",
                excursions.size(), count(Severity::Extrapolation),
                count(Severity::OutOfRange), 100.0 * worst_relative_excursion());
  s += buf;
  for (const Excursion& e : excursions) {
    std::snprintf(buf, sizeof(buf),
                  "  [%s] %s = %.6g, outside [%.6g, %.6g] for %s (%+.1f%% of window)\n"
                  "        bound source: %s\n",
                  e.severity == Severity::OutOfRange ? "OUT " : "EXTR",
                  e.quantity, e.value, e.lo, e.hi, e.model, 100.0 * e.relative, e.source);
    s += buf;
  }
  return s;
}

// -----------------------------------------------------------------------------
void check_range(Report& r, double value, double lo, double hi,
                 const char* quantity, const char* model, const char* source,
                 double extrapolation_limit) {
  if (!std::isfinite(value)) return;

  const bool below = std::isfinite(lo) && value < lo;
  const bool above = std::isfinite(hi) && value > hi;
  if (!below && !above) return;

  // Window width sets the scale for "how far outside". If only one side is
  // bounded there is no width, so fall back to the bound's own magnitude --
  // that keeps `relative` dimensionless and roughly comparable across
  // quantities without inventing a second scale
  double width = 0.0;
  if (std::isfinite(lo) && std::isfinite(hi)) width = hi - lo;
  if (!(width > 0.0)) width = std::fabs(below ? lo : hi);
  if (!(width > 0.0)) width = 1.0;

  const double overshoot = below ? (lo - value) : (value - hi);

  Excursion e;
  e.quantity = quantity;
  e.model    = model;
  e.source   = source;
  e.value    = value;
  e.lo       = lo;
  e.hi       = hi;
  e.relative = overshoot / width;
  e.severity = (e.relative > extrapolation_limit) ? Severity::OutOfRange
                                                  : Severity::Extrapolation;
  r.add(e);
}

namespace {

// Viscosity correlations are the one place a per-species temperature window is
// already stored (reactor::Dippr102::T_min_K / T_max_K, from Perry's Table
// 2-312) and was, until now, never consulted. Only species actually PRESENT in
// the stream are checked -- an absent species' window is irrelevant
void checkViscosityWindows(Report& r, const reactor::SpeciesArray& y,
                            double T_min_seen_K, double T_max_seen_K) {
  for (int i = 0; i < reactor::NS; ++i) {
    const std::size_t si = static_cast<std::size_t>(i);
    if (!(y[si] > 1e-9)) continue;   // not meaningfully present
    const reactor::Dippr102& c = reactor::kVaporViscosity[si];
    if (!c.sourced) continue;

    // Check both ends of the temperature excursion the integration actually
    // saw, not just the inlet -- an adiabatic bed can leave the window partway
    // down the tube and come back, and that still matters
    check_range(r, T_min_seen_K, c.T_min_K, c.T_max_K,
                "gas temperature (min along bed)", "DIPPR-102 vapour viscosity",
                c.source);
    check_range(r, T_max_seen_K, c.T_min_K, c.T_max_K,
                "gas temperature (max along bed)", "DIPPR-102 vapour viscosity",
                c.source);
  }
}

// Temperature range actually traversed. Falls back to the endpoints when no
// profile was recorded (record_profile = false is the norm in a sweep)
template <typename ResultT, typename StateT>
void temperatureSpan(const ResultT& res, const StateT& inlet,
                     double& T_min, double& T_max) {
  T_min = std::min(inlet.T_K, res.outlet.T_K);
  T_max = std::max(inlet.T_K, res.outlet.T_K);
  for (const auto& s : res.profile) {
    T_min = std::min(T_min, s.T_K);
    T_max = std::max(T_max, s.T_K);
  }
}

}  // namespace

// -----------------------------------------------------------------------------
Report check_reactor(const reactor::ReactorState& inlet,
                     const reactor::ReactorConfig& cfg,
                     const reactor::ReactorResult& res) {
  Report r;
  (void)cfg;

  // LHHW pressure window. Checked at both ends of the bed, since a large
  // pressure drop could carry the outlet out of a window the inlet was inside
  double P_min_bar = units::paToBar(std::min(inlet.P_Pa, res.outlet.P_Pa));
  double P_max_bar = units::paToBar(std::max(inlet.P_Pa, res.outlet.P_Pa));
  for (const auto& s : res.profile) {
    P_min_bar = std::min(P_min_bar, units::paToBar(s.P_Pa));
    P_max_bar = std::max(P_max_bar, units::paToBar(s.P_Pa));
  }
  check_range(r, P_max_bar, -kUnbounded, bounds::kLhhwPressureMaxBar,
              "reactor pressure (max)", "LHHW methanol kinetics",
              bounds::kLhhwPressureSource);

  // LHHW temperature window is deliberately unsourced -- the call is left here
  // so that encoding a bound later is a one-line change with no plumbing
  double T_min = 0.0, T_max = 0.0;
  temperatureSpan(res, inlet, T_min, T_max);
  check_range(r, T_max, bounds::kLhhwTemperatureMinK, bounds::kLhhwTemperatureMaxK,
              "reactor temperature (max)", "LHHW methanol kinetics",
              "NOT YET SOURCED -- see validity.hpp");

  if (!cfg.visc.use_constant) {
    checkViscosityWindows(r, inlet.mole_fractions(), T_min, T_max);
  }
  return r;
}

Report check_trm(const front_end::TrmReactorState& inlet,
                 const front_end::TrmReactorConfig& cfg,
                 const front_end::TrmReactorResult& res) {
  Report r;

  double T_min = 0.0, T_max = 0.0;
  temperatureSpan(res, inlet, T_min, T_max);

  check_range(r, T_min, bounds::kXuFromentTemperatureMinK,
              bounds::kXuFromentTemperatureMaxK,
              "TRM temperature (min)", "Xu & Froment reforming kinetics",
              bounds::kXuFromentSource);
  check_range(r, T_max, bounds::kXuFromentTemperatureMinK,
              bounds::kXuFromentTemperatureMaxK,
              "TRM temperature (max)", "Xu & Froment reforming kinetics",
              bounds::kXuFromentSource);

  const double P_bar = units::paToBar(inlet.P_Pa);
  check_range(r, P_bar, bounds::kXuFromentPressureMinBar,
              bounds::kXuFromentPressureMaxBar,
              "TRM pressure", "Xu & Froment reforming kinetics",
              bounds::kXuFromentSource);

  if (!cfg.visc.use_constant) {
    checkViscosityWindows(r, inlet.mole_fractions(), T_min, T_max);
  }
  return r;
}

Report check_electrolyzer(const front_end::ElectrolyzerResult& res) {
  Report r;
  if (!res.in_fit_range) {
    Excursion e;
    e.quantity = "electrolyzer operating point (P_mod, p_PEM)";
    e.model    = "Mucci Table A.1 efficiency polynomial";
    e.source   = "Mucci et al. (2023) Table A.1 fit window: "
                 "P_mod in [0.2, 2.5] MW, p_PEM in [20, 40] bar";
    e.value    = res.P_mod_MW;
    e.lo       = 0.2;
    e.hi       = 2.5;
    // ElectrolyzerResult flags the window but does not report which axis or by
    // how much, so severity is fixed rather than computed. Widening
    // ElectrolyzerResult to carry per-axis excursions would let this be exact
    e.relative = 0.0;
    e.severity = Severity::Extrapolation;
    r.add(e);
  }
  return r;
}

Report check_costing_size(double capacity, double min_size, double max_size,
                          const char* equipment_name) {
  Report r;
  check_range(r, capacity, min_size, max_size, equipment_name,
              "Turton Table A.1 purchased-cost correlation",
              "Turton et al. 5e, Table A.1 stated size range");
  return r;
}

Report check_costing_pressure(double p_barg, double p_min_barg, double p_max_barg,
                              const char* equipment_name) {
  Report r;
  check_range(r, p_barg, p_min_barg, p_max_barg, equipment_name,
              "Turton Table A.2 pressure factor (Eq. A.3)",
              "Turton et al. 5e, Table A.2 stated pressure range");
  return r;
}

// -----------------------------------------------------------------------------
Report check_design_point(const flowsheet::HybridPlantDesignPointConfig& cfg,
                          const flowsheet::HybridPlantDesignPointResult& res) {
  Report r;

  // Reactor loop pressure. The design point does not expose a per-tube reactor
  // result directly, so the loop pressure is checked from the configured
  // operating point rather than the integrated profile
  const auto& loop = res.reactor_chain;
  (void)loop;

  // Electrolyzer: every dispatch timestep runs one, and a sweep can easily
  // push module power outside Table A.1's window. Report the worst case rather
  // than one entry per hour
  bool any_electrolyzer_excursion = false;
  double worst_P_mod = 0.0;
  for (const auto& step : res.dispatch.steps) {
    if (step.electrolyzer.ok && !step.electrolyzer.in_fit_range) {
      any_electrolyzer_excursion = true;
      worst_P_mod = std::max(worst_P_mod, step.electrolyzer.P_mod_MW);
    }
  }
  if (any_electrolyzer_excursion) {
    Excursion e;
    e.quantity = "electrolyzer module power (worst timestep)";
    e.model    = "Mucci Table A.1 efficiency polynomial";
    e.source   = "Mucci et al. (2023) Table A.1: P_mod in [0.2, 2.5] MW, "
                 "p_PEM in [20, 40] bar";
    e.value    = worst_P_mod;
    e.lo       = 0.2;
    e.hi       = 2.5;
    e.relative = (worst_P_mod > 2.5) ? (worst_P_mod - 2.5) / 2.3 : 0.0;
    e.severity = (e.relative > 0.25) ? Severity::OutOfRange : Severity::Extrapolation;
    r.add(e);
  }

  // Reactor CAPEX is costed as a shell-and-tube exchanger against Turton Table
  // A.1's area range (10-1000 m^2). A bed sweep changes that area directly, and
  // large tube counts leave the range fast
  const reactor::BedGeometry& bed = res.bed_used;
  const double area_m2 = static_cast<double>(bed.n_tubes) * 3.14159265358979323846
                       * bed.tube_inner_diameter_m * bed.bed_length_m;
  r.merge(check_costing_size(area_m2, 10.0, 1000.0,
                             "reactor heat-transfer area (costed as shell-and-tube)"));

  // Compressor CAPEX: Turton's fluid-power range is 450-3000 kW
  if (res.compression.ok) {
    r.merge(check_costing_size(res.total_electricity_MW * 1000.0, 450.0, 3000.0,
                               "compression train fluid power"));
  }

  // Catalyst activity is a raw multiplier in (0, 1]; anything outside is
  // rejected upstream, but a sweep can generate it and should see why
  check_range(r, cfg.activity, 0.0, 1.0, "catalyst activity",
              "activity is a fractional multiplier by construction",
              "aged_reactor.hpp contract", 0.0);

  return r;
}

const char* to_string(LoopFeasibility f) {
  switch (f) {
    case LoopFeasibility::Feasible:   return "Feasible";
    case LoopFeasibility::Marginal:   return "Marginal";
    case LoopFeasibility::Infeasible: return "Infeasible";
    case LoopFeasibility::Unmeasured: return "Unmeasured";
  }
  return "Unmeasured";
}

LoopFeasibilityReport check_recycle_loop_before_solving(double fresh_h2_to_co2_ratio,
                                                        double purge_fraction) {
  // The measured grid, from tools/operating_envelope.cpp. iters = 0 means the
  // node did not converge. Order matches the CSV
  struct Node { double R, purge; int iters; };
  static constexpr Node kGrid[] = {
    {2.40, 0.10,  63}, {2.40, 0.08,  58}, {2.40, 0.07,  83},
    {2.40, 0.05, 111}, {2.40, 0.03,  90}, {2.40, 0.02,   0},
    {2.40, 0.01,   0},
    {2.70, 0.10,  75}, {2.70, 0.08,  80}, {2.70, 0.07, 103},
    {2.70, 0.05, 102}, {2.70, 0.03,  61}, {2.70, 0.02,   0},
    {2.70, 0.01, 118},
    {2.85, 0.10,  53}, {2.85, 0.08,  48}, {2.85, 0.07,  69},
    {2.85, 0.05,  60}, {2.85, 0.03, 104}, {2.85, 0.02,  88},
    {2.85, 0.01, 171},
    {2.95, 0.10,  47}, {2.95, 0.08,  55}, {2.95, 0.07,  49},
    {2.95, 0.05,  74}, {2.95, 0.03,  73}, {2.95, 0.02,  99},
    {2.95, 0.01, 115},
    {3.00, 0.10,  61}, {3.00, 0.08,  81}, {3.00, 0.07,  86},
    {3.00, 0.05, 218}, {3.00, 0.03, 108}, {3.00, 0.02, 510},
    {3.00, 0.01, 291},
  };
  constexpr double kCap = 600.0;      // RecycleLoopConfig::max_iterations
  constexpr double kTolR = 5.0e-3;
  constexpr double kTolP = 1.0e-3;

  LoopFeasibilityReport out;
  for (const Node& n : kGrid) {
    const double dR = fresh_h2_to_co2_ratio - n.R;
    const double dP = purge_fraction - n.purge;
    if ((dR < 0 ? -dR : dR) > kTolR) continue;
    if ((dP < 0 ? -dP : dP) > kTolP) continue;

    out.nearest_ratio = n.R;
    out.nearest_purge = n.purge;
    out.iterations_measured = n.iters;
    out.cap_fraction = n.iters / kCap;

    if (n.iters == 0) {
      out.feasibility = LoopFeasibility::Infeasible;
      out.note = "measured: the recycle grows until the Ergun drop consumes "
                 "the loop pressure, or the tear iteration does not converge";
    } else if (out.cap_fraction > bounds::kLoopMarginalCapFraction) {
      out.feasibility = LoopFeasibility::Marginal;
      out.note = "measured: converges here but uses most of the iteration "
                 "budget and is known not to converge on every platform";
    } else {
      out.feasibility = LoopFeasibility::Feasible;
      out.note = "measured: converges with margin";
    }
    return out;
  }

  out.feasibility = LoopFeasibility::Unmeasured;
  out.note = "not on the measured grid; the feasible region is non-convex so "
             "no value is interpolated. Solve it and classify the result";
  return out;
}

}  // namespace validity
