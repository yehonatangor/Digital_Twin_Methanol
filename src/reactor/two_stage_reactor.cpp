#include "two_stage_reactor.hpp"

#include "units.hpp"

namespace reactor {

TwoStageResult integrate_two_stage(const ReactorState& inlet, const TwoStageConfig& cfg) {
  TwoStageResult res;

  // --- stage 1 --------------------------------------------------------------
  res.stage1 = integrate_reactor(inlet, cfg.stage1);
  if (!res.stage1.ok) {
    res.message = "stage 1 failed: " + res.stage1.message;
    return res;
  }

  // --- interstage condensation ---------------------------------------------
  // The condenser calls flash::solve() directly through ReactorState::toStream(),
  // so the model has one flash implementation rather than an adapter layer
  const double P_flash =
      (cfg.interstage_P_Pa > 0.0) ? cfg.interstage_P_Pa : res.stage1.outlet.P_Pa;

  try {
    res.interstage = flash::solve(res.stage1.outlet.toStream(), cfg.interstage_T_K, P_flash);
  } catch (const std::exception& e) {
    res.message = std::string("interstage flash failed: ") + e.what();
    return res;
  }

  for (const auto& [sp, n] : res.interstage.liquid.molar_flow) {
    if (n > 0.0) res.crude_liquid[idx(sp)] = n;
  }

  // --- stage 2 feed ---------------------------------------------------------
  // Mucci states no stage-2 inlet temperature; the coolant temperature is used
  // Modelling choice, not sourced
  ReactorState stage2_inlet = ReactorState::fromStream(res.interstage.vapor);
  stage2_inlet.T_K  = cfg.stage2.cooling.coolant_T_K;
  stage2_inlet.P_Pa = P_flash;
  stage2_inlet.W_kg = 0.0;

  if (!(stage2_inlet.total_flow_mol_s() > 0.0)) {
    res.message = "interstage condensation left no vapour for stage 2";
    return res;
  }

  res.stage2 = integrate_reactor(stage2_inlet, cfg.stage2);
  if (!res.stage2.ok) {
    res.message = "stage 2 failed: " + res.stage2.message;
    return res;
  }

  res.outlet = res.stage2.outlet;
  res.ok = true;
  res.message = "ok";
  return res;
}

TwoStageConfig mucci_two_stage_config(const BedGeometry& bed_stage1,
                                      const BedGeometry& bed_stage2) {
  TwoStageConfig cfg;

  // Mucci App. A.4 assumes 1 bar per stage, so ConstantTotal rather than Ergun
  // reproduces what Mucci modelled
  cfg.stage1.dp.model = PressureDropModel::ConstantTotal;
  cfg.stage2.dp.model = PressureDropModel::ConstantTotal;
  cfg.stage1.dp.constant_total_drop_Pa = 1.0e5;
  cfg.stage2.dp.constant_total_drop_Pa = 1.0e5;

  // Mucci App. A.4: evaporating water at ~245 C
  cfg.stage1.thermal = ThermalMode::Cooled;
  cfg.stage2.thermal = ThermalMode::Cooled;
  cfg.stage1.cooling.coolant_T_K = units::celsiusToKelvin(245.0);
  cfg.stage2.cooling.coolant_T_K = units::celsiusToKelvin(245.0);
  // cooling.U_W_m2K keeps the derived value from energy_balance.hpp

  cfg.stage1.bed = bed_stage1;
  cfg.stage2.bed = bed_stage2;

  return cfg;
}

}  // namespace reactor
