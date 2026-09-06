// Recycle loop plus the distillation column

#include "flowsheet/co2_h2_plant_recycle_purified.hpp"
#include "_harness.inc"

int main() {
  std::printf("=== purified recycle loop ===\n\n");

  const double co2 = flowsheet::presets::van_dal_industrial_co2_kg_s();
  const double h2  = flowsheet::presets::stoichiometric_h2_feed_kg_s(co2);

  flowsheet::PurifiedRecycleLoopConfig cfg;
  std::printf("[1] Shi's recovery and purity are the defaults\n");
  check("recovery", cfg.distillation_cfg.methanol_recovery_fraction, 0.995, 1e-12);
  check("purity",   cfg.distillation_cfg.product_purity_wt_fraction, 0.999, 1e-12);

  const auto r = flowsheet::run_co2_h2_plant_recycle_purified(co2, h2, cfg);
  std::printf("\n[2] It runs and the loop underneath still converges\n");
  checkTrue("ok", r.ok);
  checkTrue("the recycle loop converged", r.recycle.converged);
  checkTrue("the column ran", r.distillation.ok);

  std::printf("\n[3] Reported production is the DISTILLATE, not the crude\n");
  checkTrue("product is positive", r.meoh_product_kg_s > 0.0);
  checkTrue("the distillate is smaller than the crude figure",
            r.meoh_product_kg_s <= r.recycle.meoh_production_kg_s + 1e-9);
  checkTrue("but not by much, since recovery is 99.5 %",
            r.crude_vs_distillate_mass_ratio > 0.99 &&
            r.crude_vs_distillate_mass_ratio < 1.20);
  std::printf("       [INFO] crude %.4f kg/s, distillate %.4f kg/s, ratio %.4f\n",
              r.recycle.meoh_production_kg_s, r.meoh_product_kg_s,
              r.crude_vs_distillate_mass_ratio);

  std::printf("\n[3b] Intermediate let-down flash degasses the crude\n");
  auto no_ld = cfg; no_ld.use_letdown_flash = false;
  const auto plain = flowsheet::run_co2_h2_plant_recycle_purified(co2, h2, no_ld);
  checkTrue("ok without the let-down", plain.ok);
  checkTrue("the let-down is on by default", cfg.use_letdown_flash);
  check("let-down pressure, bar", cfg.letdown_P_bar, 25.0, 1e-12);
  checkTrue("the flash converged", r.letdown.ok);
  checkTrue("gas is vented", r.dissolved_gas_vented_mol_s > 0.0);

  // Hydrogen is barely soluble and comes out readily. Carbon dioxide does not,
  // because methanol is a good physical solvent for it, which is the basis of
  // the Rectisol process. Asserting BOTH stops a future change from claiming
  // the flash strips CO2 that it physically cannot
  const double h2_before  = flowOf(plain.column_feed, Species::H2);
  const double h2_after   = flowOf(r.column_feed, Species::H2);
  const double co2_before = flowOf(plain.column_feed, Species::CO2);
  const double co2_after  = flowOf(r.column_feed, Species::CO2);
  std::printf("       [INFO] H2 %.4f to %.4f, CO2 %.4f to %.4f mol/s\n",
              h2_before, h2_after, co2_before, co2_after);
  checkTrue("most of the dissolved hydrogen is removed", h2_after < 0.5 * h2_before);
  checkTrue("carbon dioxide largely stays dissolved, as it must",
            co2_after > 0.9 * co2_before);

  checkTrue("almost no methanol is lost with the vent",
            r.methanol_lost_to_letdown_mol_s < 0.001 * flowOf(r.column_feed, Species::CH3OH));
  checkRel("product is essentially unchanged by degassing",
           r.meoh_product_kg_s, plain.meoh_product_kg_s, 1e-3);
  checkTrue("the column sees less permanent gas than without the flash",
            r.distillation.light_ends_mol_s < plain.distillation.light_ends_mol_s);

  std::printf("\n[3c] Loop diagnostics\n");
  // The loop runs far hydrogen-rich of the fresh feed, because hydrogen
  // recycles while carbon is consumed. Reporting it is the point
  std::printf("       [INFO] loop SN %.2f, peak bed T %.1f K, tail gas %.1f MW, product %.1f MW\n",
              r.recycle.loop_stoichiometric_number, r.recycle.peak_bed_temperature_K,
              r.recycle.tail_gas_LHV_MW, r.recycle.product_LHV_MW);
  // The canonical point is fed near stoichiometry, so the loop does accumulate
  // hydrogen and the inlet SN sits well above the fresh-feed value. Reporting
  // that is the point of the diagnostic; it is a property of this branch, not
  // a fault
  checkTrue("the loop runs hydrogen-rich, as a near-stoichiometric feed implies",
            r.recycle.loop_stoichiometric_number > 5.0);
  checkTrue("peak bed temperature is at least the outlet",
            r.recycle.peak_bed_temperature_K >= r.recycle.reactor.outlet.T_K - 1e-9);
  // The cooled jacket produces the classic commercial profile: a hump in the
  // first third of the tube, then a decline toward the exit
  checkTrue("the cooled bed has an interior hot spot",
            r.recycle.peak_bed_temperature_K > r.recycle.reactor.outlet.T_K + 0.5);
  checkTrue("and it stays below Fichtl's fitted upper bound",
            r.recycle.peak_bed_temperature_K < 553.0);
  // Recycling to near-extinction leaves little carbon to burn, so the tail gas
  // is a minor by-product rather than half the plant's chemical energy. That is
  // what the 1 % purge buys, and what its 8x recycle is paying for
  const double to_fuel = r.recycle.tail_gas_LHV_MW
                       / (r.recycle.tail_gas_LHV_MW + r.recycle.product_LHV_MW);
  std::printf("       [INFO] tail gas %.1f MW against product %.1f MW, %.1f %% to fuel\n",
              r.recycle.tail_gas_LHV_MW, r.recycle.product_LHV_MW, 100*to_fuel);
  checkTrue("the product carries most of the chemical energy",
            r.recycle.product_LHV_MW > 10.0 * r.recycle.tail_gas_LHV_MW);
  checkTrue("less than a tenth leaves as fuel", to_fuel < 0.10);

  std::printf("\n[4] The product is genuinely pure\n");
  checkTrue("purity target met", r.distillation.actual_purity_wt_fraction >= 0.999 - 1e-6);
  checkTrue("the distillate is nearly all methanol",
            r.distillation.distillate.moleFraction(Species::CH3OH) > 0.98);

  std::printf("\n[5] The remaining scope gap is stated, not implied closed\n");
  checkTrue("the message names the uncosted column",
            r.message.find("capital cost") != std::string::npos);

  return report("purified recycle loop");
}
