// Mucci's cooled two-stage configuration
//
// What this can and cannot establish is set by the source. Mucci et al. (2023)
// App. A.4 publishes the operating data (two stages, cooled at 245 C, about
// 75 bar, 1 bar drop per stage, interstage condensation) but no stream table
// and no tube dimensions, so there is no isolated validation case to reproduce
// The header says so and this test does not pretend otherwise
//
// What is checkable is the bookkeeping: the stated pressure drop must appear at
// the outlet, mass and atoms must survive the interstage flash, and the
// unsourced interstage temperature must stay labelled unsourced

#include "reactor/two_stage_reactor.hpp"
#include "reactor/ergun.hpp"
#include "species.hpp"
#include "_harness.inc"

#include <cstddef>

int main() {
  std::printf("=== two-stage reactor, Mucci App. A.4 ===\n\n");

  const reactor::BedGeometry bed = reactor::presets::van_dal_lab_mass_primary();
  reactor::TwoStageConfig cfg = reactor::mucci_two_stage_config(bed, bed);

  std::printf("[1] The configuration carries Mucci's published operating data\n");
  check("stage 1 coolant, degC", cfg.stage1.cooling.coolant_T_K - 273.15, 245.0, 0.01);
  check("stage 2 coolant, degC", cfg.stage2.cooling.coolant_T_K - 273.15, 245.0, 0.01);
  check("stage 1 drop, bar", cfg.stage1.dp.constant_total_drop_Pa / 1e5, 1.0, 1e-12);
  check("stage 2 drop, bar", cfg.stage2.dp.constant_total_drop_Pa / 1e5, 1.0, 1e-12);
  checkTrue("both stages are cooled, not adiabatic",
            cfg.stage1.thermal == reactor::ThermalMode::Cooled &&
            cfg.stage2.thermal == reactor::ThermalMode::Cooled);
  checkTrue("the constant-drop model is used, because Mucci assumed a fixed drop",
            cfg.stage1.dp.model == reactor::PressureDropModel::ConstantTotal);

  std::printf("\n[2] The interstage temperature stays labelled unsourced\n");
  // Mucci gives no interstage temperature. The 40 C default is borrowed from a
  // final separator in a different paper, so the flag is a contract: if it ever
  // reads true, someone has claimed a source that does not exist
  checkTrue("interstage_T_sourced is false", cfg.interstage_T_sourced == false);
  check("interstage T, degC", cfg.interstage_T_K - 273.15, 40.0, 0.01);

  std::printf("\n[3] Pressure bookkeeping: 75 bar in, 1 bar per stage, 73 bar out\n");
  reactor::ReactorState inlet = reactor::van_dal_lab_feed();
  inlet.P_Pa = 75.0e5;
  const reactor::TwoStageResult r = reactor::integrate_two_stage(inlet, cfg);

  checkTrue("the two-stage integration succeeds", r.ok);
  if (!r.ok) {
    std::printf("       [INFO] message: %s\n", r.message.c_str());
    return report("two-stage reactor");
  }

  // This is arithmetic on Mucci's stated drop, not a reproduction of a
  // published outlet pressure. Mucci publishes no stream table
  check("stage 1 outlet, bar", r.stage1.outlet.P_Pa / 1e5, 74.0, 0.01);
  check("outlet, bar", r.outlet.P_Pa / 1e5, 73.0, 0.01);
  check("outlet matches the configured expectation",
        r.outlet.P_Pa, cfg.expected_outlet_P_Pa, 1.0);

  std::printf("\n[4] Mass survives the interstage flash\n");
  // The condenser removes product between the stages, so stage 2's inlet is
  // lighter than stage 1's outlet by exactly the condensate
  double liq = 0.0;
  for (std::size_t i = 0; i < r.crude_liquid.size(); ++i) {
    liq += r.crude_liquid[i] * properties(static_cast<Species>(i)).molar_mass / 1000.0;
  }
  const double in_kg_s  = inlet.mass_flow_kg_s();
  const double out_kg_s = r.outlet.mass_flow_kg_s();
  std::printf("       [INFO] in %.6g kg/s, out %.6g kg/s, condensate %.6g kg/s\n",
              in_kg_s, out_kg_s, liq);
  checkRel("inlet mass equals outlet plus interstage condensate",
           out_kg_s + liq, in_kg_s, 1e-4);
  checkTrue("the condenser actually removed something", liq > 0.0);

  std::printf("\n[5] Interstage removal buys conversion the first stage could not\n");
  // Pulling product out between the stages is the reason the configuration
  // exists: it shifts the equilibrium stage 1 ran into. Stage 2's own outlet
  // holds LESS methanol than stage 1's, because the condenser took the rest,
  // so the quantity that must grow is the total made across both stages
  const std::size_t iM = static_cast<std::size_t>(Species::CH3OH);
  const double meoh_stage1 = r.stage1.outlet.F[iM];
  const double meoh_total  = r.crude_liquid[iM] + r.outlet.F[iM];
  std::printf("       [INFO] stage 1 outlet %.6g mol/s, condensate %.6g mol/s, "
              "stage 2 outlet %.6g mol/s, total %.6g mol/s\n",
              meoh_stage1, r.crude_liquid[iM], r.outlet.F[iM], meoh_total);
  checkTrue("stage 1 produces methanol", meoh_stage1 > 0.0);
  checkTrue("the two stages together beat the first alone",
            meoh_total > meoh_stage1);
  checkTrue("stage 2 ran on a real feed after condensation", r.stage2.ok);

  return report("two-stage reactor");
}
