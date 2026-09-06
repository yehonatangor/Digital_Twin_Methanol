// Feed blending to a target stoichiometric number
// Lim et al. (2022) Eq. (4): SN = (H2 - CO2) / (CO + CO2)

#include "front_end/feed_blend.hpp"
#include "_harness.inc"

#include <stdexcept>

int main() {
  std::printf("=== feed blending ===\n\n");

  std::printf("[1] Eq. (4) on a hand-checkable stream\n");
  Stream s;
  s.temperature = 500.0; s.pressure = 30e5;
  s.molar_flow[Species::H2]  = 50.0;
  s.molar_flow[Species::CO2] = 10.0;
  s.molar_flow[Species::CO]  = 10.0;
  // (50 - 10) / (10 + 10) = 2.0
  check("SN", front_end::stoichiometric_number(s), 2.0, 1e-12);

  std::printf("\n[2] The documented worked example: SN 1.25 raised to 2.0\n");
  // Syngas with SN = 1.25 needs 30 mol/s of hydrogen to reach SN = 2.0
  Stream syngas;
  syngas.temperature = 500.0; syngas.pressure = 30e5;
  syngas.molar_flow[Species::H2]  = 60.0;
  syngas.molar_flow[Species::CO2] = 10.0;
  syngas.molar_flow[Species::CO]  = 30.0;
  check("SN before top-up", front_end::stoichiometric_number(syngas), 1.25, 1e-12);

  Stream h2;
  h2.temperature = 500.0; h2.pressure = 30e5;
  h2.molar_flow[Species::H2] = 1000.0;   // ample supply to draw from

  const auto r = front_end::blend_to_target_SN(syngas, h2);
  checkTrue("ok", r.ok);
  check("SN before", r.SN_before_topup, 1.25, 1e-12);
  check("SN after",  r.SN_after,        2.00, 1e-9);
  check("hydrogen added, mol/s", r.h2_topup_mol_s, 30.0, 1e-9);
  check("blended H2 = 60 + 30", flowOf(r.blended, Species::H2), 90.0, 1e-9);

  std::printf("\n[3] Carbon is untouched by the top-up\n");
  check("CO2", flowOf(r.blended, Species::CO2), 10.0, 1e-12);
  check("CO",  flowOf(r.blended, Species::CO),  30.0, 1e-12);

  std::printf("\n[4] A stream already at target needs nothing\n");
  const auto none = front_end::blend_to_target_SN(s, h2);
  check("no hydrogen added", none.h2_topup_mol_s, 0.0, 1e-9);
  check("SN unchanged", none.SN_after, 2.0, 1e-9);

  std::printf("\n[5] Mixing conserves moles and energy\n");
  Stream a; a.temperature = 400.0; a.pressure = 1e5; a.molar_flow[Species::H2] = 10.0;
  Stream b; b.temperature = 600.0; b.pressure = 1e5; b.molar_flow[Species::H2] = 10.0;
  const Stream mix = front_end::mix_streams({a, b});
  check("moles add", flowOf(mix, Species::H2), 20.0, 1e-12);
  checkTrue("mixed temperature lies between the inlets",
            mix.temperature > 400.0 && mix.temperature < 600.0);

  std::printf("\n[6] No carbon oxides means SN is undefined, and says so loudly\n");
  Stream nocarbon; nocarbon.temperature = 500.0; nocarbon.pressure = 1e5;
  nocarbon.molar_flow[Species::H2] = 10.0;
  bool threw = false;
  try {
    (void)front_end::stoichiometric_number(nocarbon);
  } catch (const std::runtime_error&) {
    threw = true;
  }
  checkTrue("SN throws rather than returning a silent infinity", threw);

  return report("feed blending");
}
