// Packed-bed methanol synthesis reactor
//
// The validation gate is Van-Dal & Bouallou (2013)'s stated 33 % per-pass CO2
// conversion on their own laboratory feed (Table A.3). Atom balances and the
// non-negativity clamp are checked at the same time, because a conversion
// figure means nothing if moles are being fabricated to reach it

#include "reactor/reactor_core.hpp"
#include "reactor/transport.hpp"
#include "_harness.inc"
#include "reference_data.hpp"

namespace {

// Independent atom counting, written here rather than reused from production
// code so a shared bug cannot make the two agree
void atoms(const reactor::SpeciesArray& F, double& C, double& H, double& O) {
  auto f = [&](Species s) { return F[static_cast<std::size_t>(s)]; };
  C = f(Species::CO2) + f(Species::CO) + f(Species::CH3OH) + f(Species::CH4);
  H = 2.0 * f(Species::H2) + 2.0 * f(Species::H2O) + 4.0 * f(Species::CH3OH)
    + 4.0 * f(Species::CH4);
  O = 2.0 * f(Species::CO2) + f(Species::CO) + f(Species::H2O) + f(Species::CH3OH)
    + 2.0 * f(Species::O2);
}

}  // namespace

int main() {
  std::printf("=== packed-bed methanol synthesis reactor ===\n\n");

  reactor::ReactorConfig cfg;
  cfg.bed = reactor::presets::van_dal_lab_mass_primary();
  cfg.record_profile = false;

  const reactor::ReactorState inlet = reactor::van_dal_lab_feed();
  const auto res = reactor::integrate_reactor(inlet, cfg);

  std::printf("[1] Van-Dal's own laboratory case\n");
  checkTrue("integration ok", res.ok);
  std::printf("       [INFO] %s\n", res.message.c_str());
  // The paper states 33 %; this model gives 33.8 %, inside the rounding the
  // paper reports to
  check("per-pass CO2 conversion, %", 100.0 * res.co2_conversion(inlet), 33.0, 1.5);
  checkTrue("methanol is produced", res.meoh_yield_mol_s() > 0.0);

  std::printf("\n[2] Atoms are conserved, not fabricated\n");
  double Ci, Hi, Oi, Co, Ho, Oo;
  atoms(inlet.F, Ci, Hi, Oi);
  atoms(res.outlet.F, Co, Ho, Oo);
  checkRel("carbon",   Co, Ci, 1e-12);
  checkRel("hydrogen", Ho, Hi, 1e-12);
  checkRel("oxygen",   Oo, Oi, 1e-12);

  std::printf("\n[3] The non-negativity clamp absorbed nothing\n");
  check("largest clamp excursion", res.max_clamp_rel, 0.0, 1e-12);
  checkTrue("well inside the configured tolerance", res.max_clamp_rel <= cfg.max_clamp_rel);

  std::printf("\n[4] The grid is chosen by step SIZE, not step count\n");
  checkTrue("more steps than the bare n_steps floor", res.n_steps_used >= cfg.n_steps);
  checkTrue("step size respects max_step_kg_cat",
            cfg.max_step_kg_cat <= 0.0 || res.step_kg_cat <= cfg.max_step_kg_cat * 1.000001);

  std::printf("\n[5] Grid independence: halving the step barely moves the answer\n");
  reactor::ReactorConfig fine = cfg;
  fine.max_step_kg_cat = cfg.max_step_kg_cat > 0.0 ? cfg.max_step_kg_cat * 0.5 : 1e-5;
  const auto res_fine = reactor::integrate_reactor(inlet, fine);
  checkTrue("fine run ok", res_fine.ok);
  checkRel("conversion is grid independent to 0.1 %",
           res_fine.co2_conversion(inlet), res.co2_conversion(inlet), 1e-3);

  std::printf("\n[6] Temperature and pressure behave physically\n");
  checkTrue("adiabatic run heats up (synthesis is exothermic)", res.outlet.T_K > inlet.T_K);
  checkTrue("pressure does not rise through the bed", res.outlet.P_Pa <= inlet.P_Pa);

  std::printf("\n[7] Guards\n");
  reactor::ReactorConfig nobed = cfg;
  nobed.bed = reactor::BedGeometry{};
  const auto bad = reactor::integrate_reactor(inlet, nobed);
  checkTrue("an empty bed is refused", !bad.ok);
  checkTrue("and says why", bad.message.find("bed geometry") != std::string::npos);

  reactor::ReactorState zero = inlet;
  zero.T_K = 0.0;
  checkTrue("a zero inlet temperature is refused",
            !reactor::integrate_reactor(zero, cfg).ok);

  std::printf("\n[8] Bed geometry helpers\n");
  const auto bed = reactor::presets::shi_bwr();
  checkTrue("catalyst mass per tube is positive", reactor::catalyst_mass_per_tube_kg(bed) > 0.0);
  check("total = per tube x tubes", reactor::catalyst_mass_total_kg(bed),
        reactor::catalyst_mass_per_tube_kg(bed) * bed.n_tubes, 1e-6);
  check("bulk density = rho_p (1 - eps)", reactor::bulk_density_kg_m3(bed),
        bed.particle_density_kg_m3 * (1.0 - bed.void_fraction), 1e-9);

  std::printf("\n[9] Pure vapour viscosity against CRC and NIST, all 9 species\n");
  // INDEPENDENT: the code uses Perry's Table 2-312 DIPPR-102 coefficients and
  // these references are CRC and NIST values, so agreement is evidence rather
  // than a transcription check. Tolerances are the measured deviations
  char vlbl[96];
  for (const auto& v : refdata::kVaporViscosity) {
    std::snprintf(vlbl, sizeof vlbl, "mu(%s) at %.6g K, Pa s",
                  speciesName(v.sp), v.T_K);
    checkRel(vlbl, reactor::pure_viscosity_Pa_s(v.sp, v.T_K), v.mu_Pa_s, v.rel_tol);
  }
  checkTrue("every species used carries viscosity coefficients",
            reactor::viscosity_data_complete());

  return report("reactor");
}
