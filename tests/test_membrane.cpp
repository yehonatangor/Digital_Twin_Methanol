// Purge-gas hydrogen membrane. Mucci et al. (2023) p.16: 90 % H2 recovery

#include "front_end/membrane.hpp"
#include "_harness.inc"

int main() {
  std::printf("=== purge membrane ===\n\n");

  Stream purge;
  purge.temperature = 308.15; purge.pressure = 78e5;
  purge.molar_flow[Species::H2]  = 100.0;
  purge.molar_flow[Species::CO2] = 20.0;
  purge.molar_flow[Species::CO]  = 5.0;
  purge.molar_flow[Species::CH4] = 2.0;

  const auto r = front_end::separate_h2_membrane(purge);

  std::printf("[1] Sourced recovery fraction\n");
  check("Mucci recovery", front_end::presets::kMucciH2RecoveryFraction, 0.90, 1e-12);

  std::printf("\n[2] Ninety per cent of the hydrogen crosses, nothing else does\n");
  checkTrue("ok", r.ok);
  check("permeate H2",  flowOf(r.permeate, Species::H2),  90.0, 1e-9);
  check("retentate H2", flowOf(r.retentate, Species::H2), 10.0, 1e-9);
  check("recovered, mol/s", r.h2_recovered_mol_s, 90.0, 1e-9);
  check("permeate carries no CO2", r.permeate.molar_flow.count(Species::CO2) ?
        flowOf(r.permeate, Species::CO2) : 0.0, 0.0, 1e-12);
  check("retentate keeps all the CO2", flowOf(r.retentate, Species::CO2), 20.0, 1e-9);
  check("retentate keeps all the CO",  flowOf(r.retentate, Species::CO),   5.0, 1e-9);
  check("retentate keeps all the CH4", flowOf(r.retentate, Species::CH4),  2.0, 1e-9);

  std::printf("\n[3] Every atom is accounted for\n");
  check("total moles are conserved",
        r.permeate.totalMolarFlow() + r.retentate.totalMolarFlow(),
        purge.totalMolarFlow(), 1e-9);
  check("mass is conserved",
        r.permeate.totalMassFlow() + r.retentate.totalMassFlow(),
        purge.totalMassFlow(), 1e-12);

  std::printf("\n[4] Recovery is configurable\n");
  front_end::MembraneConfig full; full.h2_recovery_fraction = 1.0;
  const auto r2 = front_end::separate_h2_membrane(purge, full);
  check("all hydrogen crosses at 100 %", flowOf(r2.permeate, Species::H2), 100.0, 1e-9);
  front_end::MembraneConfig none; none.h2_recovery_fraction = 0.0;
  const auto r3 = front_end::separate_h2_membrane(purge, none);
  check("none crosses at 0 %", r3.h2_recovered_mol_s, 0.0, 1e-12);

  return report("membrane");
}
