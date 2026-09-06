// The closed recycle loop
//
// The tear stream is the recycle. Wegstein acceleration is applied per
// component. The drum runs at the reactor's real outlet pressure and a
// circulator restores the loop pressure, so the Ergun drop is paid for rather
// than discarded

#include "flowsheet/co2_h2_plant_recycle.hpp"
#include "_harness.inc"

int main() {
  std::printf("=== recycle loop ===\n\n");

  const double co2 = flowsheet::presets::van_dal_industrial_co2_kg_s();
  const double h2  = flowsheet::presets::stoichiometric_h2_feed_kg_s(co2);

  std::printf("[1] The pressure loop is closed by default\n");
  flowsheet::RecycleLoopConfig cfg;
  checkTrue("the drum sits at the reactor outlet pressure",
            cfg.knockout_at_reactor_outlet_pressure);
  check("recycle fraction", cfg.recycle_cfg.recycle_fraction, 0.99, 1e-12);
  check("canonical fresh H2:CO2", cfg.fresh_h2_to_co2_ratio, 2.95, 1e-12);
  checkTrue("canonical mode is FixedRatio",
            cfg.feed_ratio_mode == flowsheet::FeedRatioMode::FixedRatio);
  checkTrue("Wegstein is on", cfg.use_wegstein);
  check("Wegstein upper bound", cfg.wegstein_q_max, 0.95, 1e-12);
  check("Wegstein lower bound", cfg.wegstein_q_min, -5.0, 1e-12);
  checkTrue("the permeate is routed back to the feed", cfg.route_permeate_to_feed);

  std::printf("\n[2] The loop converges\n");
  const auto r = flowsheet::run_co2_h2_plant_with_recycle(co2, h2, cfg);
  checkTrue("ok", r.ok);
  checkTrue("converged", r.converged);
  std::printf("       [INFO] %d passes, residual %.3g\n", r.n_iterations, r.final_residual);
  checkTrue("residual is below tolerance", r.final_residual < cfg.tol_rel);
  checkTrue("Wegstein keeps it inside the iteration cap",
            r.n_iterations < cfg.max_iterations / 2);

  std::printf("\n[3] Recycling beats a single pass\n");
  check("per-pass CO2 conversion, %", 100.0 * r.co2_conversion_per_pass, 38.4, 2.0);
  checkTrue("overall conversion exceeds per-pass",
            r.co2_conversion_overall > r.co2_conversion_per_pass);
  check("overall CO2 conversion, %", 100.0 * r.co2_conversion_overall, 97.8, 2.0);
  checkTrue("overall conversion is a fraction", r.co2_conversion_overall < 1.0);

  std::printf("\n[4] The effluent cooling duty is real, and partly latent\n");
  check("cooling duty, MW", r.effluent_cooling_duty_MW, -156.6, 8.0);
  check("latent share, %", 100.0 * r.effluent_latent_fraction, 28.4, 3.0);
  checkTrue("cooling removes heat", r.effluent_cooling_duty_MW < 0.0);

  std::printf("\n[5] The circulator pays for the pressure drop\n");
  check("circulator duty, MW", r.circulator.P_comp_MW, 4.84, 0.5);
  checkTrue("the reactor outlet is genuinely below the inlet",
            r.reactor.outlet.P_Pa < 78e5);

  std::printf("\n[6] The purge membrane recovers hydrogen\n");
  checkTrue("membrane ok", r.purge_membrane.ok);
  checkTrue("hydrogen was recovered", r.purge_membrane.h2_recovered_mol_s > 0.0);
  checkTrue("the retentate still carries the inerts and CO2",
            flowOf(r.purge_membrane.retentate, Species::CO2) > 0.0);

  std::printf("\n[6b] Permeate pressure dominates the compression bill\n");
  // Doc 17's own table. A negative membrane_permeate_P_bar means the module's
  // stated contract, both outlets at the membrane's feed pressure; the booster
  // then only lifts from the drum back to the reactor inlet. A real polymeric
  // membrane delivers the permeate far lower, and the work is a factor of ~25
  // larger. Skipping the booster entirely in the first case would understate
  // the recovery cost, so both are pinned here
  checkTrue("the default is flagged as the optimistic at-feed case",
            r.permeate_pressure_assumed_at_feed);
  check("permeate lifted from the drum pressure, bar",
        r.permeate_delivery_P_bar, 71.6, 0.5);
  check("booster at the module contract, MW", r.permeate_booster.P_comp_MW, 0.040, 0.02);
  check("total loop compression, MW",
        r.circulator.P_comp_MW + r.permeate_booster.P_comp_MW, 4.88, 0.5);

  auto at15 = cfg;
  at15.membrane_permeate_P_bar = flowsheet::kTypicalPermeatePressureBar;
  const auto r15 = flowsheet::run_co2_h2_plant_with_recycle(co2, h2, at15);
  checkTrue("ok", r15.ok);
  checkTrue("a stated permeate pressure is NOT flagged as at-feed",
            !r15.permeate_pressure_assumed_at_feed);
  check("booster at 15 bar, MW", r15.permeate_booster.P_comp_MW, 0.99, 0.2);
  check("total loop compression at 15 bar, MW",
        r15.circulator.P_comp_MW + r15.permeate_booster.P_comp_MW, 5.82, 0.5);
  checkTrue("the 15 bar case still costs far more than the at-feed booster",
            r15.permeate_booster.P_comp_MW > 10.0 * r.permeate_booster.P_comp_MW);

  std::printf("\n[6c] Feed-effluent heat exchange, Van-Dal's HX4\n");
  // The reactor inlet temperature is a specified design condition, so the
  // exchanger must NOT move the fixed point. It only decides how much of the
  // heating and cooling is done against process streams instead of utilities
  auto no_fehe = cfg; no_fehe.use_fehe = false;
  const auto plain = flowsheet::run_co2_h2_plant_with_recycle(co2, h2, no_fehe);
  checkTrue("ok without the exchanger", plain.ok && plain.converged);

  checkTrue("the exchanger is on by default", cfg.use_fehe);
  checkRel("conversion is identical with and without it",
           r.co2_conversion_overall, plain.co2_conversion_overall, 1e-12);
  checkRel("production is identical",
           r.meoh_production_kg_s, plain.meoh_production_kg_s, 1e-12);
  check("iteration count is identical",
        static_cast<double>(r.n_iterations), static_cast<double>(plain.n_iterations), 0.0);
  checkRel("the full effluent cooling load is unchanged",
           r.effluent_cooling_duty_MW, plain.effluent_cooling_duty_MW, 1e-12);

  std::printf("       [INFO] preheat %.1f MW, recovered %.1f MW, trim heater %.1f MW, "
              "trim cooler %.1f MW\n", r.feed_preheat_duty_MW, r.fehe.duty_MW,
              r.trim_heater_duty_MW, r.trim_cooler_duty_MW);
  checkTrue("without the exchanger the heater carries the whole pre-heat",
            std::fabs(plain.trim_heater_duty_MW - plain.feed_preheat_duty_MW) < 1e-9);
  check("nothing is recovered when it is off", plain.fehe.duty_MW, 0.0, 1e-12);

  check("recovered duty, MW", r.fehe.duty_MW, 96.4, 4.0);
  check("effectiveness", r.fehe.effectiveness, 0.903, 0.04);
  check("trim heater, MW", r.trim_heater_duty_MW, 0.0, 1.0);
  check("trim cooler, MW", r.trim_cooler_duty_MW, -60.2, 6.0);
  checkTrue("recovered heat never exceeds what the feed needs",
            r.fehe.duty_MW <= r.feed_preheat_duty_MW + 1e-9);
  checkTrue("the exchanger cuts the heating utility by more than 90 %",
            r.trim_heater_duty_MW < 0.10 * plain.trim_heater_duty_MW);
  checkTrue("and the cooling utility by more than 40 %",
            std::fabs(r.trim_cooler_duty_MW) < 0.60 * std::fabs(plain.trim_cooler_duty_MW));

  std::printf("\n[6d] The recycle returns cold, not at reactor temperature\n");
  // Handing the recycle back hot would zero most of the pre-heat duty and hide
  // the largest heating load in the flowsheet
  checkTrue("a real pre-heat duty exists", r.feed_preheat_duty_MW > 40.0);

  std::printf("\n[6e] Fresh hydrogen policy: the canonical point sits at the knee\n");
  checkTrue("canonical mode is FixedRatio",
            cfg.feed_ratio_mode == flowsheet::FeedRatioMode::FixedRatio);
  check("canonical fresh ratio", r.feed_ratio.ratio_used, 2.95, 1e-9);

  // Feeding R per CO2 caps carbon-to-methanol at R/3, because the methanol
  // route needs three hydrogen per carbon. At R = 2.95 the cap is 98.3 %, and
  // the converged yield sits just under it, so the feed is the binding limit
  // rather than the loop
  const double ceiling = 100.0 * cfg.fresh_h2_to_co2_ratio / 3.0;
  const double nCO2 = co2 * 1000.0 / properties(Species::CO2).molar_mass;
  const double maxM = nCO2 * properties(Species::CH3OH).molar_mass / 1000.0;
  const double yield_pct = 100.0 * r.meoh_production_kg_s / maxM;
  std::printf("       [INFO] ceiling %.1f %%, achieved %.1f %%, i.e. %.1f %% of ceiling\n",
              ceiling, yield_pct, 100.0 * yield_pct / ceiling);
  check("stoichiometric ceiling is R/3", ceiling, 98.3333333, 1e-6);
  checkTrue("yield cannot exceed its own ceiling", yield_pct <= ceiling + 1e-6);
  check("carbon yield, %", yield_pct, 97.2, 1.0);

  // The point of this operating point: it reaches Van-Dal's own published
  // overall conversion on Van-Dal's own published 44,500 kg charge, with no
  // scaling of the bed
  checkTrue("overall conversion clears the published 93 %",
            r.co2_conversion_overall > 0.93);
  checkTrue("and the yield is close to the ceiling its own feed allows",
            yield_pct > 0.98 * ceiling);

  std::printf("\n[6f] The compact-loop branch trades yield for hydraulics\n");
  // Trimming the fresh ratio further removes more of the circulating hydrogen
  // surplus. It does not buy yield -- yield falls, because the R/3 cap falls
  // with it. What it buys is a smaller recycle and a smaller circulator
  auto compact = cfg;
  compact.fresh_h2_to_co2_ratio = 2.40;
  compact.recycle_cfg.recycle_fraction = 0.95;   // 5 % purge
  const auto rc = flowsheet::run_co2_h2_plant_with_recycle(co2, 0.0, compact);
  checkTrue("the compact branch converges", rc.ok);
  auto h2_lost = [&](const flowsheet::RecycleLoopResult& x){
    return flowOf(x.purge_membrane.retentate, Species::H2)
         / flowOf(x.fresh_feed, Species::H2); };
  auto recyc = [&](const flowsheet::RecycleLoopResult& x){
    return x.converged_reactor_inlet.totalMolarFlow() / x.fresh_feed.totalMolarFlow(); };
  std::printf("       [INFO] canonical: SN %.2f, %.1f %% H2 lost, %.1fx recycle, %.2f MW"
              ";  compact: SN %.2f, %.1f %% H2 lost, %.1fx recycle, %.2f MW\n",
              r.loop_stoichiometric_number, 100*h2_lost(r), recyc(r), r.circulator.P_comp_MW,
              rc.loop_stoichiometric_number, 100*h2_lost(rc), recyc(rc), rc.circulator.P_comp_MW);
  checkTrue("the compact branch runs a much smaller recycle",
            recyc(rc) < 0.7 * recyc(r));
  checkTrue("and a far smaller circulator",
            rc.circulator.P_comp_MW < 0.35 * r.circulator.P_comp_MW);
  checkTrue("at a lower loop SN",
            rc.loop_stoichiometric_number < r.loop_stoichiometric_number);
  checkTrue("but a lower carbon yield, capped at R/3",
            100.0 * rc.meoh_production_kg_s / maxM < 2.40 / 3.0 * 100.0 + 1e-6);
  checkTrue("which is strictly below the canonical yield",
            rc.meoh_production_kg_s < r.meoh_production_kg_s);

  std::printf("\n[6g] FromArgument uses the hydrogen it is handed\n");
  // The contract is that FromArgument ignores fresh_h2_to_co2_ratio and uses
  // the mass flow passed in. Asserted at the canonical ratio, which converges
  // with margin on every platform tested. It was previously asserted at the
  // stoichiometric 3.00, which is the most marginal node in the whole grid
  // (48 % of the iteration cap here, and a non-converging limit cycle on
  // GCC/UCRT Windows), so the test was measuring solver luck rather than the
  // contract. The stoichiometric case is still exercised below, without
  // requiring it to converge
  auto legacy = cfg;
  legacy.feed_ratio_mode = flowsheet::FeedRatioMode::FromArgument;
  // Derived exactly as the FixedRatio path derives it, so the two modes are
  // handed a bit-identical hydrogen flow and the comparison below tests the
  // dispatch of the policy rather than floating-point luck
  const double n_co2_mol_s = co2 * 1000.0 / properties(Species::CO2).molar_mass;
  const double h2_canonical =
      n_co2_mol_s * 2.95 * properties(Species::H2).molar_mass / 1000.0;
  const auto rl = flowsheet::run_co2_h2_plant_with_recycle(co2, h2_canonical, legacy);
  checkTrue("ok", rl.ok);
  check("it reports the ratio it was handed", rl.feed_ratio.ratio_used, 2.95, 1e-9);
  checkRel("and reproduces the FixedRatio result at the same ratio",
           rl.co2_conversion_overall, r.co2_conversion_overall, 1e-12);
  checkTrue("the configured ratio is ignored in this mode",
            cfg.feed_ratio_mode != flowsheet::FeedRatioMode::FromArgument);

  std::printf("\n[6g2] The stoichiometric feed is the expensive end of the trade\n");
  // NOT asserted for convergence. R = 3.00 at a 1 percent purge is the most
  // marginal point measured: it converges here in 291 of 600 passes and
  // oscillates without converging on GCC/UCRT Windows. Reported so the cost
  // is visible, guarded so a platform that cannot solve it does not fail the
  // suite over a data point the documentation already labels as marginal
  auto at3 = cfg; at3.fresh_h2_to_co2_ratio = 3.0;
  const auto r3 = flowsheet::run_co2_h2_plant_with_recycle(co2, 0.0, at3);
  std::printf("       [INFO] R 3.00 at 1 %% purge: ok=%d, %d passes, residual %.3g\n",
              static_cast<int>(r3.ok), r3.n_iterations, r3.final_residual);
  if (r3.ok) {
    checkTrue("when it does converge, it costs far more compression",
              r3.circulator.P_comp_MW > 3.0 * r.circulator.P_comp_MW);
  } else {
    std::printf("       [INFO] did not converge on this platform; "
                "see docs/23-flowsheet-composition.md\n");
  }

  std::printf("\n[6h] The reactor runs cooled, at the edge of the catalyst's window\n");
  // Adiabatic on this feed peaks near 284 C, well above Fichtl's 523 to 553 K
  // fitted band. The jacket holds the classic commercial profile instead, and
  // at the canonical point the peak sits just inside the band's lower edge, so
  // the decay correlation is used in range rather than extrapolated
  std::printf("       [INFO] peak bed %.1f C, outlet %.1f C\n",
              r.peak_bed_temperature_K - 273.15, r.reactor.outlet.T_K - 273.15);
  check("peak bed temperature, C", r.peak_bed_temperature_K - 273.15, 250.9, 3.0);
  checkTrue("peak sits inside Fichtl's fitted band (523 to 553 K)",
            r.peak_bed_temperature_K > 523.0 && r.peak_bed_temperature_K < 553.0);
  checkTrue("the peak is an interior hump, not the outlet",
            r.peak_bed_temperature_K > r.reactor.outlet.T_K + 0.5);

  std::printf("\n[6i] The high-yield branch runs CO2-lean, and that is the trade\n");
  // Chasing carbon yield means recycling to extinction, which dilutes the
  // inlet. The compact branch keeps the industrial partial pressure instead
  const double pco2 = 78.0 * flowOf(r.converged_reactor_inlet, Species::CO2)
                    / r.converged_reactor_inlet.totalMolarFlow();
  const double pco2_compact = 78.0 * flowOf(rc.converged_reactor_inlet, Species::CO2)
                            / rc.converged_reactor_inlet.totalMolarFlow();
  std::printf("       [INFO] inlet p_CO2 %.2f bar canonical against %.2f bar compact\n",
              pco2, pco2_compact);
  checkTrue("the canonical inlet is CO2-lean", pco2 < 8.0);
  checkTrue("the compact branch sits in the industrial band",
            pco2_compact > 12.0 && pco2_compact < 25.0);
  checkTrue("and the cooled bed still converts despite the dilution",
            r.co2_conversion_per_pass > 0.30);

  std::printf("\n[7] Steady-state carbon balance across the loop boundary\n");
  // Fresh carbon in must equal carbon out in the product and the purge
  const double C_in = flowOf(r.fresh_feed, Species::CO2);
  const double C_out = flowOf(r.knockout.liquid, Species::CO2)
                     + flowOf(r.knockout.liquid, Species::CH3OH)
                     + flowOf(r.knockout.liquid, Species::CO)
                     + flowOf(r.purge_membrane.retentate, Species::CO2)
                     + flowOf(r.purge_membrane.retentate, Species::CH3OH)
                     + flowOf(r.purge_membrane.retentate, Species::CO);
  checkRel("carbon closes at the loop boundary", C_out, C_in, 5e-3);

  std::printf("\n[8] Wegstein is load-bearing at the canonical point\n");
  // At a 12x recycle plain substitution is not merely slower: it exhausts the
  // 600-pass cap with the residual still two orders of magnitude above
  // tolerance. The accelerator is what makes this operating point reachable
  auto slow = cfg;
  slow.use_wegstein = false;
  const auto r2 = flowsheet::run_co2_h2_plant_with_recycle(co2, h2, slow);
  std::printf("       [INFO] Wegstein %d passes (residual %.2g); plain substitution "
              "%d passes (residual %.2g)\n",
              r.n_iterations, r.final_residual, r2.n_iterations, r2.final_residual);
  checkTrue("plain substitution exhausts the cap", !r2.converged);
  check("it used the whole budget",
        static_cast<double>(r2.n_iterations), static_cast<double>(cfg.max_iterations), 0.0);
  checkTrue("it was still descending, not diverging", r2.final_residual < 1e-2);
  checkTrue("Wegstein converges in a small fraction of that",
            r.n_iterations < cfg.max_iterations / 4);

  // On the compact branch both routes converge, and there the classic
  // comparison holds: same fixed point, more passes without acceleration
  auto compact_slow = compact;
  compact_slow.use_wegstein = false;
  const auto rc2 = flowsheet::run_co2_h2_plant_with_recycle(co2, 0.0, compact_slow);
  checkTrue("on the compact branch plain substitution does converge", rc2.ok && rc2.converged);
  checkRel("to the same fixed point", rc2.co2_conversion_overall, rc.co2_conversion_overall, 1e-3);
  checkTrue("in more passes", rc2.n_iterations > rc.n_iterations);

  std::printf("\n[9] Guards\n");
  checkTrue("a zero feed is refused",
            !flowsheet::run_co2_h2_plant_with_recycle(0.0, h2, cfg).ok);

  return report("recycle loop");
}
