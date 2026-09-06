// =============================================================================
// test_sampling.cpp -- validity ranges, feasibility labels, model fingerprint
// =============================================================================
// These three modules exist to make the twin safe to use as a DATA GENERATOR
// They are tested together because they are used together: every row a sweep
// writes should carry a validity flag, a feasibility label, and the fingerprint
// of the model that produced it
//
// The tests below deliberately include cases that FAIL on purpose -- a
// classifier that has never seen a failure is not a classifier
// =============================================================================

#include "sampling/validity.hpp"
#include "sampling/feasibility.hpp"
#include "sampling/fingerprint.hpp"

#include "co2_h2_plant.hpp"
#include "units.hpp"

#include <cmath>
#include <cstdio>
#include <string>

namespace {

int failures = 0;

void check(const char* what, double got, double want, double tol) {
  const double err = std::fabs(got - want) / std::fabs(want);
  const bool ok = err <= tol;
  if (!ok) ++failures;
  std::printf("[%s] %-62s got %12.6g  want %12.6g\n",
              ok ? "PASS" : "FAIL", what, got, want);
}

void checkTrue(const char* what, bool cond) {
  if (!cond) ++failures;
  std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", what);
}

// -----------------------------------------------------------------------------
void testCheckRange() {
  std::printf("\n[1] validity::check_range() mechanics\n");

  validity::Report r;
  validity::check_range(r, 50.0, 10.0, 100.0, "q", "m", "s");
  checkTrue("a value inside the window records nothing", r.within_validated_range());

  validity::check_range(r, 110.0, 10.0, 100.0, "q", "m", "s");
  checkTrue("a value above the window records an excursion", !r.within_validated_range());
  check("relative excursion = overshoot / window width", r.worst_relative_excursion(),
        10.0 / 90.0, 1e-9);
  checkTrue("modest overshoot is classified Extrapolation, not OutOfRange",
            r.count(validity::Severity::Extrapolation) == 1 &&
            r.count(validity::Severity::OutOfRange) == 0);

  validity::Report r2;
  validity::check_range(r2, 500.0, 10.0, 100.0, "q", "m", "s");
  checkTrue("a large overshoot escalates to OutOfRange",
            r2.count(validity::Severity::OutOfRange) == 1);

  validity::Report r3;
  validity::check_range(r3, -5.0, 10.0, 100.0, "q", "m", "s");
  checkTrue("undershoot is caught as well as overshoot", !r3.within_validated_range());

  // One-sided bounds must not trip on the unbounded side
  validity::Report r4;
  validity::check_range(r4, 1e9, -validity::kUnbounded, validity::kUnbounded, "q", "m", "s");
  checkTrue("an entirely unbounded check never fires", r4.within_validated_range());

  validity::Report r5;
  validity::check_range(r5, 1e9, -validity::kUnbounded, 100.0, "q", "m", "s");
  checkTrue("a one-sided upper bound still fires", !r5.within_validated_range());

  // NaN must not be silently treated as in-range
  validity::Report r6;
  validity::check_range(r6, std::nan(""), 10.0, 100.0, "q", "m", "s");
  checkTrue("a non-finite value records nothing (caller must check ok separately)",
            r6.within_validated_range());

  validity::Report merged;
  merged.merge(r);
  merged.merge(r2);
  checkTrue("merge() accumulates across reports", merged.excursions.size() == 2);
}

// -----------------------------------------------------------------------------
void testReactorValidity() {
  std::printf("\n[2] Reactor validity vs the LHHW pressure window\n");

  // Van-Dal's own loop runs at 78 bar against a kinetic model the same paper
  // says is validated to 75 bar. That mild excursion is real and inherited
  // from the source -- the checker must SEE it, because a sweep that raises
  // pressure further compounds it
  reactor::ReactorConfig cfg;
  cfg.bed = reactor::presets::van_dal_lab_mass_primary();
  cfg.record_profile = false;

  reactor::ReactorState inlet = reactor::van_dal_lab_feed();
  inlet.P_Pa = units::barToPa(78.0);
  const reactor::ReactorResult res = reactor::integrate_reactor(inlet, cfg);
  checkTrue("78 bar reactor run ok (precondition)", res.ok);

  const validity::Report r78 = validity::check_reactor(inlet, cfg, res);
  checkTrue("78 bar is flagged against the LHHW 75 bar window",
            !r78.within_validated_range());
  std::printf("       [INFO] %s", r78.summary().c_str());

  // Inside the window, the same run must come back clean
  reactor::ReactorState inlet50 = reactor::van_dal_lab_feed();   // 50 bar, Table A.3
  const reactor::ReactorResult res50 = reactor::integrate_reactor(inlet50, cfg);
  checkTrue("50 bar reactor run ok (precondition)", res50.ok);
  const validity::Report r50 = validity::check_reactor(inlet50, cfg, res50);
  checkTrue("50 bar (inside the window) reports no pressure excursion",
            r50.within_validated_range());
}

// -----------------------------------------------------------------------------
void testTrmValidity() {
  std::printf("\n[3] TRM validity vs the Xu & Froment window\n");

  // Aboosadi's own optimised case is 1100 K / 20 bar. Xu & Froment's Table 1
  // steam-reforming experiments cover 773-848 K and 3-15 bar. Aboosadi
  // extrapolates on BOTH axes and this project follows them -- which is
  // defensible, but must not be invisible
  front_end::TrmReactorConfig cfg;
  cfg.catalyst_mass_total_kg =
      reactor::catalyst_mass_per_tube_kg(front_end::presets::aboosadi_tri_reformer_bed());
  cfg.thermal  = reactor::ThermalMode::Adiabatic;
  cfg.dp.model = reactor::PressureDropModel::None;
  cfg.record_profile = false;

  const front_end::TrmReactorState inlet = front_end::aboosadi_optimized_feed();
  const front_end::TrmReactorResult res = front_end::integrate_trm_reactor(inlet, cfg);
  checkTrue("Aboosadi TRM run ok (precondition)", res.ok);

  const validity::Report r = validity::check_trm(inlet, cfg, res);
  checkTrue("the Aboosadi case is flagged as outside Xu & Froment's window",
            !r.within_validated_range());
  checkTrue("both temperature and pressure excursions are recorded",
            r.excursions.size() >= 2);
  std::printf("       [INFO] %s", r.summary().c_str());
}

// -----------------------------------------------------------------------------
void testCostingValidity() {
  std::printf("\n[4] Turton size/pressure windows (stored but previously unchecked)\n");

  const validity::Report ok_r =
      validity::check_costing_size(500.0, 10.0, 1000.0, "HX area");
  checkTrue("500 m2 inside Table A.1's 10-1000 m2 range is clean",
            ok_r.within_validated_range());

  const validity::Report big =
      validity::check_costing_size(5000.0, 10.0, 1000.0, "HX area");
  checkTrue("5000 m2 is flagged (Table A.1 tops out at 1000)",
            !big.within_validated_range());

  const validity::Report p =
      validity::check_costing_pressure(200.0, 5.0, 140.0, "HX shell pressure");
  checkTrue("200 barg is flagged against Table A.2's 5-140 barg range",
            !p.within_validated_range());
}

// -----------------------------------------------------------------------------
void testFeasibility() {
  std::printf("\n[5] Feasibility classification -- including deliberate failures\n");

  // Success path
  reactor::ReactorConfig cfg;
  cfg.bed = reactor::presets::van_dal_lab_mass_primary();
  cfg.record_profile = false;
  const reactor::ReactorState inlet = reactor::van_dal_lab_feed();
  const auto good = feasibility::classify_reactor(reactor::integrate_reactor(inlet, cfg));
  checkTrue("a converged run classifies Feasible",
            good.outcome == feasibility::Outcome::Feasible && good.feasible);

  // Deliberate temperature-bound failure: squeeze T_max_K under the known
  // adiabatic rise so the guard must trip
  reactor::ReactorConfig hot = cfg;
  hot.T_max_K = 500.0;   // adiabatic outlet is ~553 K
  const auto tfail = feasibility::classify_reactor(reactor::integrate_reactor(inlet, hot));
  checkTrue("a temperature-bound trip classifies ReactorTemperatureBound",
            tfail.outcome == feasibility::Outcome::ReactorTemperatureBound);
  checkTrue("...and is labelled infeasible", !tfail.feasible);
  checkTrue("...and retains the original message verbatim",
            tfail.raw_message.find("temperature") != std::string::npos);

  // Deliberate geometry failure
  reactor::ReactorConfig nobed = cfg;
  nobed.bed = reactor::BedGeometry{};   // all zero -> no catalyst mass
  const auto gfail = feasibility::classify_reactor(reactor::integrate_reactor(inlet, nobed));
  checkTrue("an empty bed classifies ReactorGeometryInvalid",
            gfail.outcome == feasibility::Outcome::ReactorGeometryInvalid);

  // Deliberate clamp violation: disable the size floor and use an absurd grid
  reactor::ReactorConfig coarse = cfg;
  coarse.max_step_kg_cat = 0.0;
  coarse.n_steps = 2;
  const auto cfail = feasibility::classify_reactor(reactor::integrate_reactor(inlet, coarse));
  checkTrue("an absurd grid does not classify as Feasible", !cfail.feasible);
  std::printf("       [INFO] coarse-grid outcome = %s\n",
              feasibility::to_string(cfail.outcome));

  // Label stability contract -- these codes end up in stored datasets
  checkTrue("Feasible is code 0", feasibility::to_code(feasibility::Outcome::Feasible) == 0);
  checkTrue("to_string() returns a token with no comma/space",
            std::string(feasibility::to_string(feasibility::Outcome::ReactorTemperatureBound))
                .find_first_of(", \t\"") == std::string::npos);
}

// -----------------------------------------------------------------------------
void testLoopFeasibility() {
  std::printf("\n[7] Recycle-loop feasibility, checked before solving\n");
  using validity::LoopFeasibility;

  // Feasible: converges with margin
  const auto design = validity::check_recycle_loop_before_solving(2.95, 0.01);
  checkTrue("the design point is measured feasible",
            design.feasibility == LoopFeasibility::Feasible);
  check("it reports the node it consulted", design.nearest_ratio, 2.95, 1e-12);

  // Marginal: converges here, does not converge on every platform
  const auto stoich = validity::check_recycle_loop_before_solving(3.00, 0.01);
  checkTrue("the stoichiometric node is flagged Marginal",
            stoich.feasibility == LoopFeasibility::Marginal);
  checkTrue("and reports most of the budget was used",
            stoich.cap_fraction > validity::bounds::kLoopMarginalCapFraction);

  // Infeasible: measured non-convergence
  const auto dead = validity::check_recycle_loop_before_solving(2.40, 0.01);
  checkTrue("a measured non-converging node is Infeasible",
            dead.feasibility == LoopFeasibility::Infeasible);
  checkTrue("with no iteration count", dead.iterations_measured == 0);

  // The non-convex hole is the reason this is a lookup and not a rule: 2.70
  // closes at 1 % and 3 % but not at 2 %
  checkTrue("R 2.70 at 3 % purge is feasible",
            validity::check_recycle_loop_before_solving(2.70, 0.03).feasibility
            == LoopFeasibility::Feasible);
  checkTrue("R 2.70 at 2 % purge is infeasible",
            validity::check_recycle_loop_before_solving(2.70, 0.02).feasibility
            == LoopFeasibility::Infeasible);
  checkTrue("R 2.70 at 1 % purge is feasible again, so no interval describes it",
            validity::check_recycle_loop_before_solving(2.70, 0.01).feasibility
            == LoopFeasibility::Feasible);

  // The 7 and 8 percent purge columns exist because chapter 17 quotes the
  // reference plant's 93 percent conversion from the 7 percent node
  const auto vd = validity::check_recycle_loop_before_solving(3.00, 0.07);
  checkTrue("the node chapter 17 quotes for Van-Dal's conversion is feasible",
            vd.feasibility == LoopFeasibility::Feasible);
  check("and it is the measured node, not a neighbour", vd.nearest_purge, 0.07, 1e-12);

  // Off-grid points are not interpolated across the hole
  const auto off = validity::check_recycle_loop_before_solving(2.78, 0.01);
  checkTrue("an unmeasured point is Unmeasured, not guessed",
            off.feasibility == LoopFeasibility::Unmeasured);
  checkTrue("every outcome has a stable token",
            std::string(validity::to_string(off.feasibility)) == "Unmeasured");
}

void testFingerprint() {
  std::printf("\n[6] Model fingerprint\n");

  const auto a = fingerprint::compute();
  const auto b = fingerprint::compute();

  checkTrue("fingerprint is deterministic within a process", a.hash == b.hash);
  checkTrue("fingerprint is non-trivial", a.hash != 0);
  checkTrue("hex form is 16 characters", a.hex.size() == 16);
  checkTrue("a meaningful number of constants was hashed", a.n_values > 200);

  // Subsystem hashes must actually differ from one another -- if they were all
  // equal the domain separators would not be working and the localisation
  // benefit would be illusory
  checkTrue("subsystem hashes are distinct",
            a.thermo_hash != a.kinetics_hash &&
            a.kinetics_hash != a.transport_hash &&
            a.transport_hash != a.phase_hash &&
            a.phase_hash != a.numerics_hash);

  std::printf("%s", fingerprint::report().c_str());
  std::printf("       [INFO] record this hash alongside any generated dataset\n");
}

}  // namespace

int main() {
  std::printf("=== methanol_twin sampling-support validation ===\n");
  testCheckRange();
  testReactorValidity();
  testTrmValidity();
  testCostingValidity();
  testFeasibility();
  testLoopFeasibility();
  testFingerprint();
  std::printf("\n%s -- %d failure(s)\n", failures ? "FAILED" : "ALL PASSED", failures);
  return failures ? 1 : 0;
}
