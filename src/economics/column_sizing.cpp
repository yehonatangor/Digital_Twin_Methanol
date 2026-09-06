#include "column_sizing.hpp"

#include <algorithm>
#include <cmath>

#include "eos.hpp"
#include "nrtl.hpp"
#include "species.hpp"

namespace economics {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kFtPerS_to_mPerS = 0.3048;
constexpr double kRgas = 8.314462618;

// Van-Dal & Bouallou (2013), Sec. 2.3.3: 44 rectifying and 13 stripping stages
constexpr int    kVanDalStages = 57;
constexpr double kVanDalRefluxRatio = 1.2;
}  // namespace

namespace presets {
// Turton Table 21.7, constants in Eq. (21.58). Source given there as
// Wankat, P., "Separation Processes," 4th ed., Prentice Hall, 2017
TraySpacingConstants fair_wankat_6in()  { return {1.1977,  0.53143, 0.18760,  6.0}; }
TraySpacingConstants fair_wankat_9in()  { return {1.1622,  0.56014, 0.18168,  9.0}; }
TraySpacingConstants fair_wankat_12in() { return {1.0674,  0.55780, 0.17919, 12.0}; }
TraySpacingConstants fair_wankat_18in() { return {1.0262,  0.63513, 0.20097, 18.0}; }
TraySpacingConstants fair_wankat_24in() { return {0.94506, 0.70234, 0.22618, 24.0}; }
TraySpacingConstants fair_wankat_36in() { return {0.85984, 0.73980, 0.23735, 36.0}; }
}  // namespace presets

// -----------------------------------------------------------------------------
// Part 1: the Turton equations
// -----------------------------------------------------------------------------
double flooding_flow_parameter(double L_mol_s, double V_mol_s, double ML_kg_per_mol,
                                double MV_kg_per_mol, double rho_V_kg_m3, double rho_L_kg_m3) {
  if (!(V_mol_s > 0.0) || !(MV_kg_per_mol > 0.0) || !(rho_L_kg_m3 > 0.0) || !(rho_V_kg_m3 > 0.0)) {
    return 0.0;
  }
  // Eq. (21.59)
  return (L_mol_s * ML_kg_per_mol) / (V_mol_s * MV_kg_per_mol)
         * std::sqrt(rho_V_kg_m3 / rho_L_kg_m3);
}

double flooding_capacity_factor(double Flv, const TraySpacingConstants& cfg) {
  if (!(Flv > 0.0)) return 0.0;
  const double x = std::log10(Flv);
  // Eq. (21.58)
  return std::pow(10.0, -cfg.a - cfg.b * x - cfg.c * x * x);
}

double flooding_velocity_m_s(double Csb_flood, double rho_L_kg_m3, double rho_V_kg_m3,
                              double sigma_dyne_per_cm) {
  const double drho = rho_L_kg_m3 - rho_V_kg_m3;
  if (!(drho > 0.0) || !(rho_V_kg_m3 > 0.0) || !(Csb_flood > 0.0)) return 0.0;
  // Eq. (21.60) solved for uf. Native units are ft/s
  const double sigma_term = std::pow(sigma_dyne_per_cm / 20.0, 0.2);
  const double uf_ft_s = Csb_flood * std::sqrt(drho / rho_V_kg_m3) / sigma_term;
  return uf_ft_s * kFtPerS_to_mPerS;
}

ColumnDiameterResult size_column_diameter(double L_mol_s, double V_mol_s, double ML_kg_per_mol,
                                           double MV_kg_per_mol, double rho_L_kg_m3,
                                           double rho_V_kg_m3,
                                           const TraySpacingConstants& tray_spacing_cfg,
                                           double fraction_flooding, double active_area_fraction,
                                           double sigma_dyne_per_cm) {
  ColumnDiameterResult r;

  if (!(V_mol_s > 0.0) || !(rho_L_kg_m3 > rho_V_kg_m3) || !(rho_V_kg_m3 > 0.0)) {
    r.message = "need V > 0 and rho_L > rho_V > 0";
    return r;
  }
  if (!(fraction_flooding > 0.0) || fraction_flooding > 1.0 ||
      !(active_area_fraction > 0.0) || active_area_fraction > 1.0) {
    r.message = "fraction_flooding and active_area_fraction must be in (0, 1]";
    return r;
  }

  r.Flv = flooding_flow_parameter(L_mol_s, V_mol_s, ML_kg_per_mol, MV_kg_per_mol,
                                   rho_V_kg_m3, rho_L_kg_m3);
  if (!(r.Flv > 0.0)) {
    r.message = "flow parameter is non-positive; check L, V and the molar masses";
    return r;
  }

  r.Csb_flood = flooding_capacity_factor(r.Flv, tray_spacing_cfg);
  r.uf_m_s    = flooding_velocity_m_s(r.Csb_flood, rho_L_kg_m3, rho_V_kg_m3, sigma_dyne_per_cm);
  if (!(r.uf_m_s > 0.0)) {
    r.message = "flooding velocity is non-positive";
    return r;
  }

  r.u_actual_m_s = fraction_flooding * r.uf_m_s;

  // Eq. (21.61) and the actual-area step
  const double V_kg_s = V_mol_s * MV_kg_per_mol;
  r.active_area_m2 = V_kg_s / (rho_V_kg_m3 * r.u_actual_m_s);
  r.actual_area_m2 = r.active_area_m2 / active_area_fraction;
  r.diameter_m     = std::sqrt(4.0 * r.actual_area_m2 / kPi);

  r.ok = true;
  r.message = "ok";
  return r;
}

// -----------------------------------------------------------------------------
// Part 2: bubble point
// -----------------------------------------------------------------------------
namespace {

// sum_i x_i gamma_i(x, T) Psat_i(T) - P
double bubble_residual(const Stream& liquid, double T, double P_Pa, bool& usable) {
  usable = false;
  Stream s = liquid;
  s.temperature = T;
  const auto gamma = nrtl::activityCoefficients(s, T);

  double sum = 0.0;
  for (const auto& kv : liquid.molar_flow) {
    const Species sp = kv.first;
    const double x = liquid.moleFraction(sp);
    if (!(x > 0.0)) continue;
    if (vaporPressureParams(sp) == nullptr) continue;   // no DIPPR data: skip, flagged by caller
    const auto it = gamma.find(sp);
    const double g = (it != gamma.end() && it->second > 0.0) ? it->second : 1.0;
    sum += x * g * vaporPressure(sp, T);
    usable = true;
  }
  return sum - P_Pa;
}

}  // namespace

BubblePointResult bubble_point_T_K(const Stream& liquid, double P_Pa, double T_guess_K) {
  BubblePointResult r;

  if (!(P_Pa > 0.0) || !(liquid.totalMolarFlow() > 0.0)) {
    r.message = "need a positive pressure and a non-empty liquid stream";
    return r;
  }

  // Bracket first, so the Newton step always has a fallback
  double lo = 200.0, hi = 700.0;
  bool ok_lo = false, ok_hi = false;
  const double f_lo = bubble_residual(liquid, lo, P_Pa, ok_lo);
  const double f_hi = bubble_residual(liquid, hi, P_Pa, ok_hi);
  if (!ok_lo || !ok_hi) {
    r.message = "no species in the stream has DIPPR vapour-pressure data";
    return r;
  }
  if (f_lo * f_hi > 0.0) {
    r.message = "bubble point is not bracketed in 200-700 K at this pressure";
    return r;
  }

  double T = std::min(std::max(T_guess_K, lo + 1.0), hi - 1.0);
  bool usable = false;

  for (int i = 0; i < 100; ++i) {
    const double f = bubble_residual(liquid, T, P_Pa, usable);
    if (std::abs(f) < 1e-6 * P_Pa) {
      r.T_K = T; r.n_iterations = i; r.converged = true; r.ok = true; r.message = "ok";
      return r;
    }

    // Keep the bracket current
    if (f * f_lo > 0.0) lo = T; else hi = T;

    // Newton step on a numerical derivative; fall back to bisection whenever
    // the step leaves the bracket
    const double h = 0.01;
    bool u2 = false;
    const double fp = (bubble_residual(liquid, T + h, P_Pa, u2) - f) / h;
    double T_next = (std::abs(fp) > 0.0) ? T - f / fp : 0.5 * (lo + hi);
    if (!(T_next > lo) || !(T_next < hi) || !std::isfinite(T_next)) {
      T_next = 0.5 * (lo + hi);
    }
    T = T_next;
  }

  r.T_K = T;
  r.n_iterations = 100;
  r.message = "bubble point did not converge in 100 iterations";
  return r;
}

// -----------------------------------------------------------------------------
// Part 3: the wrapper
// -----------------------------------------------------------------------------
namespace {

// Mass density from this project's own Peng-Robinson root at (T, P)
double pr_mass_density_kg_m3(const Stream& s, double T, double P_Pa, eos::RootSelect which) {
  Stream t = s;
  t.temperature = T;
  t.pressure = P_Pa;
  const auto mix = eos::mixtureParams(t, T);
  const double Z = eos::compressibilityFactor(mix, T, P_Pa, which);
  if (!(Z > 0.0)) return -1.0;
  const double v_m3_per_mol = eos::molarVolume(Z, T, P_Pa);
  if (!(v_m3_per_mol > 0.0)) return -1.0;

  // Mean molar mass, kg/mol
  double M_kg_per_mol = 0.0;
  for (const auto& kv : s.molar_flow) {
    const double x = s.moleFraction(kv.first);
    if (x > 0.0) M_kg_per_mol += x * properties(kv.first).molar_mass / 1000.0;
  }
  if (!(M_kg_per_mol > 0.0)) return -1.0;
  return M_kg_per_mol / v_m3_per_mol;
}

double mean_molar_mass_kg_per_mol(const Stream& s) {
  double M = 0.0;
  for (const auto& kv : s.molar_flow) {
    const double x = s.moleFraction(kv.first);
    if (x > 0.0) M += x * properties(kv.first).molar_mass / 1000.0;
  }
  return M;
}

}  // namespace

DistillationColumnSizingResult size_distillation_column(
    const Stream& crude_feed, const front_end::DistillationResult& distillation,
    const DistillationColumnSizingConfig& cfg) {
  DistillationColumnSizingResult res;

  if (!distillation.ok) {
    res.message = "distillation result is not ok; nothing to size";
    return res;
  }

  const double P_Pa = cfg.column_pressure_bar * 1.0e5;

  res.F_mol_s = crude_feed.totalMolarFlow();
  res.D_mol_s = distillation.distillate.totalMolarFlow();
  res.B_mol_s = distillation.bottoms.totalMolarFlow();
  if (!(res.D_mol_s > 0.0) || !(res.F_mol_s > 0.0)) {
    res.message = "feed and distillate must both carry positive molar flow";
    return res;
  }

  // Internal traffic from the condenser balance, at Van-Dal's reflux ratio
  // Saturated-liquid feed, so the vapour rate is unchanged across the feed
  // stage and the liquid rate gains the whole feed
  res.L_top_mol_s    = kVanDalRefluxRatio * res.D_mol_s;
  res.V_top_mol_s    = (kVanDalRefluxRatio + 1.0) * res.D_mol_s;
  res.V_bottom_mol_s = res.V_top_mol_s;
  res.L_bottom_mol_s = res.L_top_mol_s + (cfg.feed_is_saturated_liquid ? res.F_mol_s : 0.0);

  // Real bubble points at the column pressure
  res.top_bubble_point    = bubble_point_T_K(distillation.distillate, P_Pa, cfg.T_guess_K);
  res.bottom_bubble_point = bubble_point_T_K(distillation.bottoms,    P_Pa, cfg.T_guess_K);
  if (!res.top_bubble_point.ok || !res.bottom_bubble_point.ok) {
    res.message = "bubble-point solve failed: " + (res.top_bubble_point.ok
                                                       ? res.bottom_bubble_point.message
                                                       : res.top_bubble_point.message);
    return res;
  }

  const double T_top = res.top_bubble_point.T_K;
  const double T_bot = res.bottom_bubble_point.T_K;

  res.rho_V_top_kg_m3    = pr_mass_density_kg_m3(distillation.distillate, T_top, P_Pa, eos::RootSelect::Vapor);
  res.rho_L_top_kg_m3    = pr_mass_density_kg_m3(distillation.distillate, T_top, P_Pa, eos::RootSelect::Liquid);
  res.rho_V_bottom_kg_m3 = pr_mass_density_kg_m3(distillation.bottoms,    T_bot, P_Pa, eos::RootSelect::Vapor);
  res.rho_L_bottom_kg_m3 = pr_mass_density_kg_m3(distillation.bottoms,    T_bot, P_Pa, eos::RootSelect::Liquid);

  if (!(res.rho_V_top_kg_m3 > 0.0) || !(res.rho_L_top_kg_m3 > 0.0) ||
      !(res.rho_V_bottom_kg_m3 > 0.0) || !(res.rho_L_bottom_kg_m3 > 0.0)) {
    res.message = "Peng-Robinson root selection failed at one of the column ends";
    return res;
  }

  const double M_top = mean_molar_mass_kg_per_mol(distillation.distillate);
  const double M_bot = mean_molar_mass_kg_per_mol(distillation.bottoms);

  // Turton's own Example 21.4 uses one molar mass for both phases on a tray
  res.top_section = size_column_diameter(res.L_top_mol_s, res.V_top_mol_s, M_top, M_top,
                                          res.rho_L_top_kg_m3, res.rho_V_top_kg_m3,
                                          cfg.tray_spacing_cfg, cfg.fraction_flooding,
                                          cfg.active_area_fraction, cfg.sigma_dyne_per_cm);
  res.bottom_section = size_column_diameter(res.L_bottom_mol_s, res.V_bottom_mol_s, M_bot, M_bot,
                                             res.rho_L_bottom_kg_m3, res.rho_V_bottom_kg_m3,
                                             cfg.tray_spacing_cfg, cfg.fraction_flooding,
                                             cfg.active_area_fraction, cfg.sigma_dyne_per_cm);

  if (!res.top_section.ok || !res.bottom_section.ok) {
    res.message = "diameter calculation failed: " +
                  (res.top_section.ok ? res.bottom_section.message : res.top_section.message);
    return res;
  }

  // Size for the limiting section
  res.diameter_m = std::max(res.top_section.diameter_m, res.bottom_section.diameter_m);

  res.ok = true;
  res.message = "ok";
  return res;
}

}  // namespace economics
