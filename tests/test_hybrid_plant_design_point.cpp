// The single-call design point: geometry and activity in, economics out

#include "flowsheet/hybrid_plant_design_point.hpp"
#include "flowsheet/co2_h2_plant.hpp"
#include "sampling/feasibility.hpp"
#include "sampling/validity.hpp"
#include "_harness.inc"
#include "_hybrid_common.inc"

int main() {
  std::printf("=== design point ===\n\n");

  flowsheet::HybridPlantDesignPointConfig cfg;
  cfg.co2_feed_kg_s = flowsheet::presets::van_dal_industrial_co2_kg_s();
  cfg.dispatch_cfg.electrolyzer_power_MW = 2.0;
  cfg.dispatch_cfg.price_threshold_USD_per_MWh = 40.0;
  cfg.dispatch_cfg.storage.V_m3 = 5000.0;

  const auto r = flowsheet::evaluate_design_point(demoPrices(), demoStorage(), cfg);

  std::printf("[1] One call produces the whole picture\n");
  checkTrue("ok", r.ok);
  checkTrue("the loop converged", r.reactor_chain.recycle.converged);
  checkTrue("compression ran", r.compression.ok);
  checkTrue("capex ok", r.capex.ok);
  checkTrue("opex ok", r.annual_opex.ok);

  std::printf("\n[2] The swept variables are echoed back as actually used\n");
  check("activity used", r.activity_used, 1.0, 1e-12);
  check("bed resolved to the composite preset",
        static_cast<double>(r.bed_used.n_tubes), 6204.0, 1.0);
  std::printf("       [INFO] %.1f %% conversion, %.0f t/y, $%.1f/t\n",
              100.0 * r.co2_conversion_overall, r.annual_production_t,
              r.unit_cost_USD_per_t);

  std::printf("\n[3] Activity is a genuine input, not a cost line bolted on\n");
  auto aged_cfg = cfg;
  aged_cfg.activity = 0.4;
  const auto aged = flowsheet::evaluate_design_point(demoPrices(), demoStorage(), aged_cfg);
  checkTrue("ok", aged.ok);
  check("activity echoed", aged.activity_used, 0.4, 1e-12);
  checkTrue("a decayed catalyst produces less",
            aged.meoh_product_kg_s < r.meoh_product_kg_s);
  checkTrue("and therefore costs more per tonne",
            aged.unit_cost_USD_per_t > r.unit_cost_USD_per_t);
  std::printf("       [INFO] a=0.4: %.0f t/y at $%.1f/t\n",
              aged.annual_production_t, aged.unit_cost_USD_per_t);

  std::printf("\n[4] Geometry is a genuine input too\n");
  auto big = cfg;
  big.bed = flowsheet::presets::van_dal_catalyst_shi_tubes();
  big.bed.n_tubes = static_cast<int>(big.bed.n_tubes * 1.5);
  const auto rbig = flowsheet::evaluate_design_point(demoPrices(), demoStorage(), big);
  checkTrue("ok", rbig.ok);
  check("the swept bed is the one used",
        static_cast<double>(rbig.bed_used.n_tubes), static_cast<double>(big.bed.n_tubes), 0.0);
  checkTrue("a bigger bed converts at least as much",
            rbig.co2_conversion_overall >= r.co2_conversion_overall - 1e-6);

  std::printf("\n[5] Feasibility and validity read the result cleanly\n");
  const auto cls = feasibility::classify(r);
  checkTrue("a converged design point is feasible", cls.feasible);
  checkTrue("and classifies as Feasible", cls.outcome == feasibility::Outcome::Feasible);
  const auto vr = validity::check_design_point(cfg, r);
  // This bed is far outside Turton's costing range, and the model must say so
  // rather than quietly extrapolating
  checkTrue("range excursions are reported", !vr.excursions.empty());

  std::printf("\n[6] Guards\n");
  auto bad = cfg; bad.activity = 0.0;
  checkTrue("activity 0 is refused",
            !flowsheet::evaluate_design_point(demoPrices(), demoStorage(), bad).ok);
  bad = cfg; bad.co2_feed_kg_s = 0.0;
  checkTrue("a zero feed is refused",
            !flowsheet::evaluate_design_point(demoPrices(), demoStorage(), bad).ok);

  return report("design point");
}
