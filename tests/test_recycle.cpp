// Recycle/purge splitter. Mucci et al. (2023) p.16: 98 % recycle

#include "front_end/recycle.hpp"
#include "_harness.inc"

int main() {
  std::printf("=== recycle/purge split ===\n\n");

  Stream gas;
  gas.temperature = 308.15; gas.pressure = 78e5;
  gas.molar_flow[Species::H2]  = 100.0;
  gas.molar_flow[Species::CO2] = 20.0;

  std::printf("[1] Sourced default\n");
  check("Mucci recycle fraction", front_end::presets::kMucciRecycleFraction, 0.98, 1e-12);

  std::printf("\n[2] The split is proportional and composition preserving\n");
  front_end::RecycleConfig cfg{0.70};
  const auto r = front_end::split_recycle_purge(gas, cfg);
  checkTrue("ok", r.ok);
  check("recycle H2",  flowOf(r.recycle, Species::H2),   70.0, 1e-9);
  check("purge H2",    flowOf(r.purge, Species::H2),     30.0, 1e-9);
  check("recycle CO2", flowOf(r.recycle, Species::CO2),  14.0, 1e-9);
  check("purge CO2",   flowOf(r.purge, Species::CO2),     6.0, 1e-9);
  check("H2 mole fraction is unchanged in the recycle",
        r.recycle.moleFraction(Species::H2), gas.moleFraction(Species::H2), 1e-12);
  check("and in the purge",
        r.purge.moleFraction(Species::H2), gas.moleFraction(Species::H2), 1e-12);

  std::printf("\n[3] Conservation\n");
  check("moles", r.recycle.totalMolarFlow() + r.purge.totalMolarFlow(),
        gas.totalMolarFlow(), 1e-9);
  check("mass",  r.recycle.totalMassFlow() + r.purge.totalMassFlow(),
        gas.totalMassFlow(), 1e-12);

  std::printf("\n[4] Temperature and pressure ride along unchanged\n");
  check("recycle T", r.recycle.temperature, gas.temperature, 1e-12);
  check("purge P",   r.purge.pressure,      gas.pressure,    1e-12);

  std::printf("\n[5] Endpoints\n");
  const auto none = front_end::split_recycle_purge(gas, front_end::RecycleConfig{0.0});
  check("nothing recycled at 0", none.recycle.totalMolarFlow(), 0.0, 1e-12);
  const auto all = front_end::split_recycle_purge(gas, front_end::RecycleConfig{1.0});
  check("nothing purged at 1", all.purge.totalMolarFlow(), 0.0, 1e-12);

  return report("recycle split");
}
