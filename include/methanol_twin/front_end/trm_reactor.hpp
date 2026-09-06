#pragma once
// Tri-reforming packed-bed reactor
//
// Xu & Froment (1989) reactions I, II, III plus Trimm & Lam methane
// combustion, with Aboosadi et al. (2011) parameters and effectiveness
// factors eta = 0.07 / 0.06 / 0.7 / 0.05 (Aboosadi Table 3)
// RK4 over catalyst mass, same integrator convention as reactor/
//
// VALIDITY. Xu & Froment Table 1 fits 773-848 K and 3-15 bar. Aboosadi's own
// validation case runs at 1100 K and 20 bar, outside both. This project
// follows them and sampling/validity.hpp reports the excursion
//
// PLACEHOLDER: bed void fraction 0.5 and particle density 1775 kg/m3 are not
// stated in either source paper; 1775 is borrowed from Van-Dal. Flagged in
// the preset's own message string
// See docs/14-tri-reforming-reactor.md

#include <array>
#include <string>
#include <vector>

// Included to reuse reactor::ReactorState and the generic bed/transport types
#include "reactor/reactor_core.hpp"
#include "species.hpp"
#include "stream.hpp"

namespace front_end {

inline constexpr int N_TRM_RXN = 4;

// Indices match Aboosadi's R1-R4 labelling
enum TrmRxn : int {
  RXN_SRM        = 0,  // CH4 + H2O  <-> CO  + 3H2   (Xu & Froment reaction I)
  RXN_OVERALL    = 1,  // CH4 + 2H2O <-> CO2 + 4H2   (Xu & Froment reaction III = I+II)
  RXN_WGS        = 2,  // CO  + H2O  <-> CO2 + H2    (Xu & Froment reaction II)
  RXN_COMBUSTION = 3   // CH4 + 2O2  <-> CO2 + 2H2O  (Trimm & Lam, Ni-adjusted)
};

// nu[reaction][species]. Aboosadi Eqs. (1), (4)-(6)
inline constexpr std::array<std::array<double, static_cast<std::size_t>(reactor::NS)>,
                             N_TRM_RXN> kTrmStoich{{
  //  CO2   H2    CO    H2O   CH3OH  CH4   N2    Ar    O2
  {   0.0,  3.0,  1.0, -1.0,  0.0,  -1.0,  0.0,  0.0,  0.0 },  // RXN_SRM
  {   1.0,  4.0,  0.0, -2.0,  0.0,  -1.0,  0.0,  0.0,  0.0 },  // RXN_OVERALL
  {   1.0,  1.0, -1.0, -1.0,  0.0,   0.0,  0.0,  0.0,  0.0 },  // RXN_WGS
  {   1.0,  0.0,  0.0,  2.0,  0.0,  -1.0,  0.0,  0.0, -2.0 },  // RXN_COMBUSTION
}};

// Intraparticle transport limitation. De Groote & Froment (1996) Appl. Catal
// A 138, 245-264, as used in Aboosadi Eqs. (12a)-(12f). Measured on a partial
// oxidation catalyst, not this one; Aboosadi's choice, carried through
struct EffectivenessFactors {
  double eta_srm        = 0.07;
  double eta_overall     = 0.06;
  double eta_wgs         = 0.7;
  double eta_combustion  = 0.05;
};

struct ArrheniusRate {
  double A;           // units vary by reaction, see the .cpp
  double E_J_mol;
};

struct VantHoffAdsorption {
  double A_bar_inv;
  double dH_J_mol;    // negative = exothermic adsorption
};

struct TrmKineticsParams {
  // Xu & Froment (1989) Table 6, kmol/(kgcat.h). Converted to mol/(kgcat.s)
  // in trm_reaction_rates(), the one place that conversion happens
  ArrheniusRate k_srm_kmol_h;       // bar^0.5 basis
  ArrheniusRate k_wgs_kmol_h;       // bar^-1 basis
  ArrheniusRate k_overall_kmol_h;   // bar^0.5 basis

  // Shared denominator terms. Xu & Froment Table 6 / Aboosadi Table 3
  VantHoffAdsorption K_CO;
  VantHoffAdsorption K_H2;
  VantHoffAdsorption K_CH4;
  VantHoffAdsorption K_H2O;

  // Aboosadi Tables 2 and 3, from Trimm & Lam (1980) adjusted to Ni per
  // De Smet (2001). Already mol/(kgcat.s)
  ArrheniusRate k4a_mol_s;
  ArrheniusRate k4b_mol_s;
  VantHoffAdsorption K_CH4_combustion;
  VantHoffAdsorption K_O2_combustion;

  // Aboosadi Table 2. Used as-is rather than from thermo::Keq(), because these
  // are the correlations the rate laws were fitted against
  double KI_a, KI_b;     // K_I = exp(KI_a / T + KI_b), bar^2
  double KIII_a, KIII_b; // K_III = exp(KIII_a / T + KIII_b), dimensionless
};

TrmKineticsParams aboosadi_kinetics_params();

// R_j [mol/(kgcat.s)] in TrmRxn order, before effectiveness factors
std::array<double, N_TRM_RXN> trm_reaction_rates(const reactor::SpeciesArray& y, double T_K,
                                                  double P_Pa, const TrmKineticsParams& kp);

// Net species rates [mol/(kgcat.s)] after eta and stoichiometry
reactor::SpeciesArray trm_species_rates(const std::array<double, N_TRM_RXN>& R,
                                         const EffectivenessFactors& eta);

// [J/mol extent], gas basis, from thermo::deltaH()
double trm_delta_H_rxn_J_per_mol(int j, double T_K);

using TrmReactorState = reactor::ReactorState;

struct TrmReactorConfig {
  // Primary control. Direct catalyst mass rather than bed-derived, so the
  // model runs without a void fraction or bulk density for this catalyst
  double catalyst_mass_total_kg = 0.0;

  // Consulted only for Ergun pressure drop or axial position reporting
  reactor::BedGeometry        bed{};
  reactor::PressureDropConfig dp{};   // None by default, per Aboosadi
  reactor::ViscosityConfig    visc{};

  reactor::ThermalMode   thermal = reactor::ThermalMode::Adiabatic;
  reactor::CoolingConfig cooling{};

  TrmKineticsParams     kinetics = aboosadi_kinetics_params();
  EffectivenessFactors  eta{};

  // -------------------------------------------------------------------------
  // Integration grid. Same reasoning as reactor::ReactorConfig -- read that
  // block too. `n_steps` is a FLOOR on the count; `max_step_kg_cat` is a
  // ceiling on the step SIZE, and the effective grid is
  //     n_effective = max(n_steps, ceil(W_end / max_step_kg_cat))
  //
  // The tri-reformer needs its own target step size rather than inheriting
  // the methanol reactor's, because the two operate on completely different
  // catalyst inventories: the Aboosadi vessel holds ~5,600 kg where a
  // methanol synthesis tube holds single-digit kg. A step size appropriate
  // for one is absurd for the other in both directions
  //
  // 1.4 kg reproduces the ~4,000 steps the Aboosadi validation case was
  // already shown to be converged at, and -- unlike the bare count -- keeps
  // that resolution if the vessel is resized, preventing divergence on 
  // extremely large industrial catalyst charges
  int  n_steps = 500;             // MINIMUM step count
  double max_step_kg_cat = 1.4;   // target maximum step, kg catalyst

  bool record_profile  = true;
  bool use_real_gas_Z  = true;

  // T_max_K exceeds reactor_core's because Aboosadi Fig. 12b shows the
  // combustion spike reaching ~1690 K
  double T_min_K  = 200.0;
  double T_max_K  = 2000.0;
  double P_min_Pa = 1.0e4;

  // See reactor::ReactorConfig::max_clamp_rel. O2 is the species a coarse grid
  // drives negative first here, so this is where clamping fabricates moles
  double max_clamp_rel = 1.0e-9;
};

struct TrmReactorResult {
  TrmReactorState outlet{};
  std::vector<TrmReactorState> profile;
  bool ok = false;
  std::string message;

  int    n_steps_used = 0;
  double step_kg_cat  = 0.0;
  double max_clamp_rel = 0.0;

  double ch4_conversion(const TrmReactorState& inlet) const;
  double co2_net_conversion(const TrmReactorState& inlet) const;  // may be negative: CO2 is a co-reactant
  double h2_co_ratio() const;
};

TrmReactorResult integrate_trm_reactor(const TrmReactorState& inlet, const TrmReactorConfig& cfg);

// Aboosadi Table 7 optimized feed: 1100 K, 20 bara, CO2 24.81 / CO 0.01 /
// H2 1.53 / CH4 18.7 / O2 8.78 / N2 0.01 / H2O 46.18 mol%, 28,115.4 kmol/h
TrmReactorState aboosadi_optimized_feed();

namespace presets {

// Aboosadi Table 6 geometry (shell ID 2 m, particle 19x16 mm 10-hole rings,
// modelled here as a single vessel, n_tubes = 1) PLUS an estimated catalyst
// mass for cases that want a BedGeometry-consistent W_end
//
// ==== PLACEHOLDER -- void fraction and particle/bulk density are NOT stated
// in Aboosadi or Cho et al. for this catalyst; only shape and particle size
// are given. The values below (void_fraction = 0.5, particle_density =
// 1775 kg/m3) are borrowed from Van-Dal's Cu/ZnO/Al2O3 methanol-synthesis
// catalyst bed (Table A.2, the lab-scale case; reactor::presets::
// van_dal_lab_mass_primary()) already used elsewhere in this project as a
// dimensionally-reasonable stand-in, NOT
// measured for this specific catalyst. Overriding them is a one-line change
// to the returned struct; nothing downstream depends on these being exact
// (see header note 3 -- Aboosadi's Fig. 12 profile plateaus well before the
// stated length, so this does not block matching Table 8's outlet)
reactor::BedGeometry aboosadi_tri_reformer_bed();
inline constexpr bool kAboosadiBedGeometrySourced = false;

}  // namespace presets

}  // namespace front_end

