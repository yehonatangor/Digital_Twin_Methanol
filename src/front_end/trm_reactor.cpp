#include "trm_reactor.hpp"

#include <algorithm>
#include <cmath>

#include "thermo.hpp"
#include "units.hpp"

namespace front_end {

// -----------------------------------------------------------------------------
// Sourced parameter set -- see trm_reactor.hpp for full citations
// -----------------------------------------------------------------------------
TrmKineticsParams aboosadi_kinetics_params() {
  TrmKineticsParams kp;

  // Xu & Froment (1989) Table 6 preexponentials in kmol/(kgcat.h), Table 5
  // activation energies. Verified by 400-dpi render plus OCR, independent of
  // the PDF text layer, and cross-checked against Aboosadi Table 2, which
  // equals these divided by 3.6 exactly
  //
  // These are Table 6's reference-activity constants, not fresh-catalyst
  // Xu & Froment p. 94 gives a factor of 2.246 for fresh catalyst. It is not
  // applied here, because Aboosadi (this module's validation case) also uses
  // Table 6 as printed, and because it was measured: applying it moves CH4
  // conversion 96.20 -> 96.49 % and every outlet fraction by under 0.15 mol%,
  // since the reactor is equilibrium-limited at 1100 K
  kp.k_srm_kmol_h     = { 4.225e15, 240100.0 };
  kp.k_wgs_kmol_h     = { 1.955e6,   67130.0 };
  kp.k_overall_kmol_h = { 1.020e15, 243900.0 };

  // Xu & Froment Table 6, bar^-1. Identical to Aboosadi Table 3
  kp.K_CO  = { 8.23e-5, -70650.0 };
  kp.K_H2  = { 6.12e-9, -82900.0 };
  kp.K_CH4 = { 6.65e-4, -38280.0 };
  kp.K_H2O = { 1.77e5,   88680.0 };

  // Aboosadi Tables 2 and 3: Trimm & Lam (1980) re-fitted to Ni per De Smet
  // (2001). Already mol/(kgcat.s)
  kp.k4a_mol_s = { 8.11e5, 86000.0 };
  kp.k4b_mol_s = { 6.82e5, 86000.0 };
  kp.K_CH4_combustion = { 1.26e-1, -27300.0 };
  kp.K_O2_combustion  = { 7.78e-7, -92800.0 };

  // Aboosadi Table 2
  //   K_I   = exp(-26,830/T + 30.114)   [bar^2],          SRM
  //   K_III = exp(  4,400/T -  4.036)   [dimensionless],  WGS
  //   K_II  = K_I * K_III                                  overall
  kp.KI_a   = -26830.0; kp.KI_b   = 30.114;
  kp.KIII_a =   4400.0; kp.KIII_b = -4.036;

  return kp;
}

// -----------------------------------------------------------------------------
// Rate laws
// -----------------------------------------------------------------------------
namespace {

constexpr double kFloor = 1e-12;

double arrhenius(const ArrheniusRate& r, double T_K) {
  return r.A * std::exp(-r.E_J_mol / (units::R * T_K));
}

double vanthoff(const VantHoffAdsorption& a, double T_K) {
  return a.A_bar_inv * std::exp(-a.dH_J_mol / (units::R * T_K));
}

}  // namespace

std::array<double, N_TRM_RXN> trm_reaction_rates(const reactor::SpeciesArray& y, double T_K,
                                                  double P_Pa, const TrmKineticsParams& kp) {
  std::array<double, N_TRM_RXN> R{};
  if (!(T_K > 0.0) || !(P_Pa > 0.0)) return R;

  const double P_bar = units::paToBar(P_Pa);
  auto pp = [&](Species sp) {
    return std::max(kFloor, y[reactor::idx(sp)] * P_bar);
  };
  const double p_CH4 = pp(Species::CH4), p_H2O = pp(Species::H2O);
  const double p_CO  = pp(Species::CO),  p_H2  = pp(Species::H2);
  const double p_CO2 = pp(Species::CO2), p_O2  = pp(Species::O2);

  const double K_I   = std::exp(kp.KI_a / T_K + kp.KI_b);
  const double K_III = std::exp(kp.KIII_a / T_K + kp.KIII_b);
  const double K_II  = K_I * K_III;

  // Shared adsorption denominator
  const double phi = 1.0 + vanthoff(kp.K_CO, T_K) * p_CO
                         + vanthoff(kp.K_H2, T_K) * p_H2
                         + vanthoff(kp.K_CH4, T_K) * p_CH4
                         + vanthoff(kp.K_H2O, T_K) * (p_H2O / p_H2);
  const double phi2 = phi * phi;

  // Unit boundary: kmol/(kgcat.h) * 1000/3600 = mol/(kgcat.s). Only here
  constexpr double kUnit = 1000.0 / 3600.0;
  const double k1 = arrhenius(kp.k_srm_kmol_h, T_K)     * kUnit;
  const double k2 = arrhenius(kp.k_wgs_kmol_h, T_K)     * kUnit;
  const double k3 = arrhenius(kp.k_overall_kmol_h, T_K) * kUnit;

  // Xu & Froment (1989) Eq. (3) form, reaction I -- SRM
  R[RXN_SRM] = (k1 / std::pow(p_H2, 2.5)) *
               (p_CH4 * p_H2O - std::pow(p_H2, 3.0) * p_CO / K_I) / phi2;

  // Xu & Froment (1989) Eq. (3) form, reaction III -- overall
  R[RXN_OVERALL] = (k3 / std::pow(p_H2, 3.5)) *
                   (p_CH4 * p_H2O * p_H2O - std::pow(p_H2, 4.0) * p_CO2 / K_II) / phi2;

  // Xu & Froment (1989) Eq. (3) form, reaction II -- WGS
  R[RXN_WGS] = (k2 / p_H2) * (p_CO * p_H2O - p_H2 * p_CO2 / K_III) / phi2;

  // Aboosadi Eq. (10), Trimm & Lam combustion: two parallel terms over one
  // linear site-balance denominator, as printed. De Smet (2001) squares the
  // denominator on the first term and Aboosadi's k4a units (bar^-2) agree with
  // that, so Eq. (10) has probably dropped an exponent. Followed as printed
  // anyway, because Aboosadi is the validation case. Measured either way: the
  // squared form halves the combustion rate but moves CH4 conversion by
  // 0.0001 and T_out by 0.04 K, because O2 burns to completion regardless
  const double denom_c = 1.0 + vanthoff(kp.K_CH4_combustion, T_K) * p_CH4
                             + vanthoff(kp.K_O2_combustion, T_K) * p_O2;
  const double k4a = arrhenius(kp.k4a_mol_s, T_K);
  const double k4b = arrhenius(kp.k4b_mol_s, T_K);
  R[RXN_COMBUSTION] = (k4a * p_CH4 * p_O2) / denom_c + (k4b * p_CH4 * p_O2) / denom_c;

  for (double& x : R) if (!std::isfinite(x)) x = 0.0;
  return R;
}

reactor::SpeciesArray trm_species_rates(const std::array<double, N_TRM_RXN>& R,
                                         const EffectivenessFactors& eta) {
  // Aboosadi Eqs. (12a)-(12f): r_i = sum_j eta_j * nu_ij * R_j, taken from
  // kTrmStoich so the stoichiometry lives in exactly one place
  const std::array<double, N_TRM_RXN> e{eta.eta_srm, eta.eta_overall, eta.eta_wgs,
                                        eta.eta_combustion};
  reactor::SpeciesArray r{};
  for (int i = 0; i < reactor::NS; ++i) {
    double s = 0.0;
    for (int j = 0; j < N_TRM_RXN; ++j) {
      s += e[static_cast<std::size_t>(j)] *
           kTrmStoich[static_cast<std::size_t>(j)][static_cast<std::size_t>(i)] *
           R[static_cast<std::size_t>(j)];
    }
    r[static_cast<std::size_t>(i)] = s;
  }
  return r;
}

double trm_delta_H_rxn_J_per_mol(int j, double T_K) {
  if (j < 0 || j >= N_TRM_RXN) return 0.0;
  double s = 0.0;
  for (int i = 0; i < reactor::NS; ++i) {
    const double nu = kTrmStoich[static_cast<std::size_t>(j)][static_cast<std::size_t>(i)];
    if (nu != 0.0) s += nu * thermo::enthalpy(static_cast<Species>(i), T_K);
  }
  return s;
}

// -----------------------------------------------------------------------------
// Generalizes energy_balance.hpp's dTdW_K_per_kg from 2 reactions to 4,
// reusing its heat-capacity and heat-removal helpers
// -----------------------------------------------------------------------------
namespace {

double trm_dTdW(const reactor::SpeciesArray& F, double T_K,
                const std::array<double, N_TRM_RXN>& R, const EffectivenessFactors& eta,
                reactor::ThermalMode mode, const reactor::CoolingConfig& cool,
                const reactor::BedGeometry& bed) {
  if (mode == reactor::ThermalMode::Isothermal) return 0.0;

  const std::array<double, N_TRM_RXN> e{eta.eta_srm, eta.eta_overall, eta.eta_wgs,
                                        eta.eta_combustion};
  double q_gen = 0.0;
  for (int j = 0; j < N_TRM_RXN; ++j) {
    q_gen += (-trm_delta_H_rxn_J_per_mol(j, T_K)) * e[static_cast<std::size_t>(j)] *
             R[static_cast<std::size_t>(j)];
  }

  const double q_rem = (mode == reactor::ThermalMode::Cooled)
                           ? reactor::heat_removal_W_per_kg(T_K, cool, bed)
                           : 0.0;

  const double sum_FCp = reactor::stream_heat_capacity_W_per_K(F, T_K);
  if (!(sum_FCp > 0.0)) return 0.0;
  return (q_gen - q_rem) / sum_FCp;
}

// RHS, same layout as reactor_core.cpp: st[0..NS-1] = F_i, st[NS] = T,
// st[NS+1] = P
constexpr std::size_t kNVar = static_cast<std::size_t>(reactor::NS) + 2;
using Vec = std::array<double, kNVar>;

Vec toVec(const TrmReactorState& s) {
  Vec v{};
  for (int i = 0; i < reactor::NS; ++i) v[static_cast<std::size_t>(i)] = s.F[static_cast<std::size_t>(i)];
  v[static_cast<std::size_t>(reactor::NS)]     = s.T_K;
  v[static_cast<std::size_t>(reactor::NS) + 1] = s.P_Pa;
  return v;
}

TrmReactorState fromVec(const Vec& v, double W) {
  TrmReactorState s;
  for (int i = 0; i < reactor::NS; ++i) s.F[static_cast<std::size_t>(i)] = v[static_cast<std::size_t>(i)];
  s.T_K  = v[static_cast<std::size_t>(reactor::NS)];
  s.P_Pa = v[static_cast<std::size_t>(reactor::NS) + 1];
  s.W_kg = W;
  return s;
}

bool trm_rhs(const Vec& v, const TrmReactorConfig& cfg, Vec& out) {
  const TrmReactorState s = fromVec(v, 0.0);
  if (!(s.T_K > 0.0) || !(s.P_Pa > 0.0) || !(s.total_flow_mol_s() > 0.0)) return false;

  const reactor::SpeciesArray y = s.mole_fractions();
  const auto R = trm_reaction_rates(y, s.T_K, s.P_Pa, cfg.kinetics);
  const reactor::SpeciesArray r = trm_species_rates(R, cfg.eta);

  for (int i = 0; i < reactor::NS; ++i) out[static_cast<std::size_t>(i)] = r[static_cast<std::size_t>(i)];

  out[static_cast<std::size_t>(reactor::NS)] =
      trm_dTdW(s.F, s.T_K, R, cfg.eta, cfg.thermal, cfg.cooling, cfg.bed);

  double dPdW = 0.0;
  if (cfg.dp.model != reactor::PressureDropModel::None) {
    const double rho = reactor::mass_density_kg_m3(s, cfg.use_real_gas_Z);
    const double G   = reactor::mass_flux_kg_m2s(s, cfg.bed);
    const double mu  = reactor::mixture_viscosity_Pa_s(y, s.T_K, cfg.visc);
    if (!(rho > 0.0) || !(mu > 0.0)) return false;
    dPdW = reactor::pressure_gradient_dPdW(cfg.dp, mu, rho, G, cfg.bed);
  }
  out[static_cast<std::size_t>(reactor::NS) + 1] = dPdW;

  for (double x : out) if (!std::isfinite(x)) return false;
  return true;
}

}  // namespace

double TrmReactorResult::ch4_conversion(const TrmReactorState& inlet) const {
  const double in = inlet.F[reactor::idx(Species::CH4)];
  return (in > 0.0) ? (in - outlet.F[reactor::idx(Species::CH4)]) / in : 0.0;
}

double TrmReactorResult::co2_net_conversion(const TrmReactorState& inlet) const {
  const double in = inlet.F[reactor::idx(Species::CO2)];
  return (in > 0.0) ? (in - outlet.F[reactor::idx(Species::CO2)]) / in : 0.0;
}

double TrmReactorResult::h2_co_ratio() const {
  const double co = outlet.F[reactor::idx(Species::CO)];
  return (co > 0.0) ? outlet.F[reactor::idx(Species::H2)] / co : 0.0;
}

TrmReactorResult integrate_trm_reactor(const TrmReactorState& inlet, const TrmReactorConfig& cfg) {
  TrmReactorResult res;

  const double W_end = cfg.catalyst_mass_total_kg;
  if (!(W_end > 0.0)) {
    res.message = "TrmReactorConfig::catalyst_mass_total_kg must be positive";
    return res;
  }
  if (!(inlet.T_K > 0.0) || !(inlet.P_Pa > 0.0) || !(inlet.total_flow_mol_s() > 0.0)) {
    res.message = "inlet state must have positive T, P and total flow";
    return res;
  }

  // n_steps floors the count, max_step_kg_cat caps the size
  int n = cfg.n_steps > 0 ? cfg.n_steps : 1;
  if (cfg.max_step_kg_cat > 0.0) {
    const double need = std::min(std::ceil(W_end / cfg.max_step_kg_cat), 5.0e6);
    n = std::max(n, static_cast<int>(need));
  }
  res.n_steps_used = n;
  const double h = W_end / static_cast<double>(n);
  res.step_kg_cat = h;

  double F_in_max = 0.0;
  for (double f : inlet.F) F_in_max = std::max(F_in_max, f);
  if (!(F_in_max > 0.0)) F_in_max = 1.0;

  Vec v = toVec(inlet);
  double W = 0.0;
  if (cfg.record_profile) {
    res.profile.reserve(static_cast<std::size_t>(n) + 1);
    res.profile.push_back(fromVec(v, W));
  }

  Vec k1{}, k2{}, k3{}, k4{}, tmp{};
  for (int step = 0; step < n; ++step) {
    if (!trm_rhs(v, cfg, k1)) { res.message = "rhs failed (k1)."; res.outlet = fromVec(v, W); return res; }
    for (std::size_t i = 0; i < kNVar; ++i) tmp[i] = v[i] + 0.5 * h * k1[i];
    if (!trm_rhs(tmp, cfg, k2)) { res.message = "rhs failed (k2)."; res.outlet = fromVec(v, W); return res; }
    for (std::size_t i = 0; i < kNVar; ++i) tmp[i] = v[i] + 0.5 * h * k2[i];
    if (!trm_rhs(tmp, cfg, k3)) { res.message = "rhs failed (k3)."; res.outlet = fromVec(v, W); return res; }
    for (std::size_t i = 0; i < kNVar; ++i) tmp[i] = v[i] + h * k3[i];
    if (!trm_rhs(tmp, cfg, k4)) { res.message = "rhs failed (k4)."; res.outlet = fromVec(v, W); return res; }

    for (std::size_t i = 0; i < kNVar; ++i) v[i] += (h / 6.0) * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
    W += h;

    // Clamp negative flows, scored against inlet: O2 runs out first here, so
    // an unmeasured clamp manufactures oxygen
    for (int i = 0; i < reactor::NS; ++i) {
      const std::size_t k = static_cast<std::size_t>(i);
      if (v[k] < 0.0) {
        res.max_clamp_rel = std::max(res.max_clamp_rel, -v[k] / F_in_max);
        v[k] = 0.0;
      }
    }
    if (res.max_clamp_rel > cfg.max_clamp_rel) {
      res.outlet = fromVec(v, W);
      res.message = "non-negativity clamp absorbed " + std::to_string(res.max_clamp_rel) +
                    " of inlet flow, above the tolerance -- the step is too large";
      return res;
    }

    const double T = v[static_cast<std::size_t>(reactor::NS)];
    const double P = v[static_cast<std::size_t>(reactor::NS) + 1];
    if (!(T > cfg.T_min_K) || !(T < cfg.T_max_K)) {
      res.outlet = fromVec(v, W);
      res.message = "temperature left the guard band at W = " + std::to_string(W);
      return res;
    }
    if (!(P > cfg.P_min_Pa)) {
      res.outlet = fromVec(v, W);
      res.message = "pressure fell below the guard at W = " + std::to_string(W);
      return res;
    }

    if (cfg.record_profile) res.profile.push_back(fromVec(v, W));
  }

  res.outlet = fromVec(v, W);
  res.ok = true;
  res.message = "ok";
  return res;
}

TrmReactorState aboosadi_optimized_feed() {
  // Aboosadi Table 7 (composition, inlet T) and Table 6 (flow, pressure)
  // Sums to 100.02 %, which is Aboosadi's own rounding. Not renormalized
  TrmReactorState s;
  s.T_K  = 1100.0;
  s.P_Pa = units::barToPa(20.0);
  s.W_kg = 0.0;

  const double n_tot = 28115.4 * 1000.0 / 3600.0;   // kmol/h -> mol/s
  s.F[reactor::idx(Species::CO2)] = 0.2481 * n_tot;
  s.F[reactor::idx(Species::CO)]  = 0.0001 * n_tot;
  s.F[reactor::idx(Species::H2)]  = 0.0153 * n_tot;
  s.F[reactor::idx(Species::CH4)] = 0.1870 * n_tot;
  s.F[reactor::idx(Species::O2)]  = 0.0878 * n_tot;
  s.F[reactor::idx(Species::N2)]  = 0.0001 * n_tot;
  s.F[reactor::idx(Species::H2O)] = 0.4618 * n_tot;
  return s;
}

namespace presets {

reactor::BedGeometry aboosadi_tri_reformer_bed() {
  reactor::BedGeometry b{};
  b.tube_inner_diameter_m  = 2.0;      // Aboosadi Table 6, shell ID, one vessel
  b.bed_length_m           = 2.0;      // Aboosadi Table 6
  b.n_tubes                = 1;
  b.particle_diameter_m    = 0.019;    // 19 x 16 mm 10-hole rings
  b.void_fraction          = 0.5;      // PLACEHOLDER -- not stated in either source paper
  b.particle_density_kg_m3 = 1775.0;   // PLACEHOLDER -- borrowed from Van-Dal
  b.source = "Aboosadi Table 6 shell/particle geometry; "
             "void fraction and particle density are UNSOURCED PLACEHOLDERS "
             "borrowed from Van-Dal's methanol catalyst";
  return b;
}

}  // namespace presets

}  // namespace front_end
