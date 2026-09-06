// Methanol purification. Shi et al. (2020): 99.5 % recovery, 99.9 wt% purity

#include "front_end/distillation.hpp"
#include "_harness.inc"

int main() {
  std::printf("=== distillation ===\n\n");

  std::printf("[1] Shi's own targets are the defaults\n");
  check("recovery", front_end::presets::kShiMethanolRecoveryFraction, 0.995,  1e-12);
  check("purity",   front_end::presets::kShiProductPurityWtFraction,  0.999,  1e-12);
  check("Mucci AA-grade purity", front_end::presets::kMucciAAGradePurityWtFraction, 0.9985, 1e-12);

  Stream crude;
  crude.temperature = 308.15; crude.pressure = 78e5;
  crude.molar_flow[Species::CH3OH] = 100.0;
  crude.molar_flow[Species::H2O]   = 100.0;
  crude.molar_flow[Species::CO2]   = 1.0;

  const auto r = front_end::purify_methanol(crude);

  std::printf("\n[2] Recovery and purity are met\n");
  checkTrue("ok", r.ok);
  check("methanol recovered, mol/s", r.methanol_recovered_mol_s, 99.5, 1e-9);
  check("distillate methanol", flowOf(r.distillate, Species::CH3OH), 99.5, 1e-9);
  check("bottoms keeps the rest", flowOf(r.bottoms, Species::CH3OH), 0.5, 1e-9);
  checkTrue("purity is at least the target",
            r.actual_purity_wt_fraction >= 0.999 - 1e-9);
  check("achieved purity", r.actual_purity_wt_fraction, 0.999, 0.001);

  std::printf("\n[3] Conservation across the column, all three outlets\n");
  check("methanol moles",
        flowOf(r.distillate, Species::CH3OH) + flowOf(r.bottoms, Species::CH3OH),
        100.0, 1e-9);
  check("total mass", r.distillate.totalMassFlow() + r.bottoms.totalMassFlow()
                      + r.light_ends.totalMassFlow(),
        crude.totalMassFlow(), 1e-9);
  check("total moles", r.distillate.totalMolarFlow() + r.bottoms.totalMolarFlow()
                       + r.light_ends.totalMolarFlow(),
        crude.totalMolarFlow(), 1e-9);

  std::printf("\n[3b] Permanent gases vent; they do not dissolve in the wastewater\n");
  // Carbon dioxide carried in with the crude cannot stay in a liquid water
  // bottoms at column conditions. Routing it there was the defect this checks
  check("the CO2 fed in leaves as vent", flowOf(r.light_ends, Species::CO2), 1.0, 1e-12);
  check("and none of it reaches the bottoms", flowOf(r.bottoms, Species::CO2), 0.0, 1e-12);
  check("nor the product", flowOf(r.distillate, Species::CO2), 0.0, 1e-12);
  check("vent total is reported", r.light_ends_mol_s, 1.0, 1e-12);
  checkTrue("the bottoms is condensables only",
            flowOf(r.bottoms, Species::H2) == 0.0 && flowOf(r.bottoms, Species::CO) == 0.0);

  std::printf("\n[3c] A clean feed vents nothing\n");
  Stream clean;
  clean.temperature = 308.15; clean.pressure = 78e5;
  clean.molar_flow[Species::CH3OH] = 100.0;
  clean.molar_flow[Species::H2O]   = 100.0;
  const auto rc = front_end::purify_methanol(clean);
  checkTrue("ok", rc.ok);
  check("no vent", rc.light_ends_mol_s, 0.0, 1e-12);

  std::printf("\n[4] Water goes to the bottoms\n");
  checkTrue("bottoms is water rich",
            flowOf(r.bottoms, Species::H2O) > 0.9 * 100.0);
  checkTrue("distillate is nearly dry",
            r.distillate.moleFraction(Species::CH3OH) > 0.99);

  std::printf("\n[5] A feed with no methanol is handled, not crashed\n");
  Stream dry;
  dry.temperature = 308.15; dry.pressure = 1e5;
  dry.molar_flow[Species::H2O] = 10.0;
  const auto r2 = front_end::purify_methanol(dry);
  checkTrue("no methanol recovered", r2.methanol_recovered_mol_s <= 1e-12);

  return report("distillation");
}
