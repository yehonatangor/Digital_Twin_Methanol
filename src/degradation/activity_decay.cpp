#include "activity_decay.hpp"

#include <cmath>
#include <stdexcept>
#include "units.hpp"

namespace activity_decay {

double Kd_per_h(double T_K, const ActivityDecayConfig& cfg) {
  if (!(T_K > 0.0)) {
    throw std::invalid_argument("activity_decay::Kd_per_h: T_K must be positive");
  }
  const double kd = cfg.A_per_h * std::exp(-cfg.Ea_J_per_mol / (units::R * T_K));
  return kd * cfg.water_multiplier;
}

double activity_closed_form(double a0, double t_h, double Kd_per_h_value, double m) {
  if (!(a0 > 0.0) || a0 > 1.0) {
    throw std::invalid_argument("activity_decay::activity_closed_form: a0 must be in (0, 1]");
  }
  if (t_h < 0.0) {
    throw std::invalid_argument("activity_decay::activity_closed_form: t_h must be non-negative");
  }
  if (Kd_per_h_value < 0.0) {
    throw std::invalid_argument("activity_decay::activity_closed_form: Kd_per_h_value must be non-negative");
  }
  if (m == 1.0) {
    // Degenerate case: da/a = -Kd dt
    return a0 * std::exp(-Kd_per_h_value * t_h);
  }

  // a(t) = [ a0^(1-m) + (m-1)*Kd*t ]^(1/(1-m))
  const double one_minus_m = 1.0 - m;
  const double base = std::pow(a0, one_minus_m) + (m - 1.0) * Kd_per_h_value * t_h;
  if (!(base > 0.0)) return 0.0; // activity driven to zero within t_h
  return std::pow(base, 1.0 / one_minus_m);
}

namespace {

double rhs(double a, double Kd_per_h_value, double m) {
  // da/dt = -Kd * a^m. Clamped so an RK4 undershoot cannot reach pow() with a negative base
  const double a_clamped = a > 0.0 ? a : 0.0;
  return -Kd_per_h_value * std::pow(a_clamped, m);
}

}

ActivityIntegrationResult integrate_activity(
    double a0, double t_end_h, int n_steps,
    const std::function<double(double t_h)>& T_of_t,
    const ActivityDecayConfig& cfg) {
  ActivityIntegrationResult res;

  if (!(a0 > 0.0) || a0 > 1.0) {
    res.message = "integrate_activity: a0 must be in (0, 1]";
    return res;
  }
  if (t_end_h < 0.0) {
    res.message = "integrate_activity: t_end_h must be non-negative";
    return res;
  }
  if (n_steps <= 0) {
    res.message = "integrate_activity: n_steps must be positive";
    return res;
  }
  if (!T_of_t) {
    res.message = "integrate_activity: T_of_t must be callable";
    return res;
  }

  const double h = t_end_h / static_cast<double>(n_steps);
  double t = 0.0;
  double a = a0;

  res.profile.reserve(static_cast<std::size_t>(n_steps) + 1);
  res.profile.push_back({t, a, T_of_t(t)});

  for (int step = 0; step < n_steps; ++step) {
    const double T1 = T_of_t(t);
    const double kd1 = Kd_per_h(T1, cfg);
    const double k1 = rhs(a, kd1, cfg.m);

    const double t_mid = t + 0.5 * h;
    const double T2 = T_of_t(t_mid);
    const double kd2 = Kd_per_h(T2, cfg);
    const double k2 = rhs(a + 0.5 * h * k1, kd2, cfg.m);
    const double k3 = rhs(a + 0.5 * h * k2, kd2, cfg.m);

    const double t_end_step = t + h;
    const double T4 = T_of_t(t_end_step);
    const double kd4 = Kd_per_h(T4, cfg);
    const double k4 = rhs(a + h * k3, kd4, cfg.m);

    a = a + (h / 6.0) * (k1 + 2.0 * k2 + 2.0 * k3 + k4);
    if (a < 0.0) a = 0.0;
    t = t_end_step;

    res.profile.push_back({t, a, T_of_t(t)});
  }

  res.a_final = a;
  res.ok = true;
  res.message = "ok";
  return res;
}

ActivityIntegrationResult integrate_activity_isothermal(
    double a0, double t_end_h, int n_steps, double T_K,
    const ActivityDecayConfig& cfg) {
  return integrate_activity(a0, t_end_h, n_steps,
                             [T_K](double /*t_h*/) { return T_K; }, cfg);
}

namespace presets {

ActivityDecayConfig fichtl_cza1_523_553K() {
  ActivityDecayConfig cfg;
  cfg.m = 3.0;
  cfg.Ea_J_per_mol = 47421.558;
  cfg.A_per_h = 78.97779;
  cfg.sourced = true;
  return cfg;
}

ActivityDecayConfig fichtl_cza2_523_553K() {
  ActivityDecayConfig cfg;
  cfg.m = 3.0;
  cfg.Ea_J_per_mol = 39008.673;
  cfg.A_per_h = 10.54438;
  cfg.sourced = true;
  return cfg;
}

ActivityDecayConfig fichtl_cza3_523_553K() {
  ActivityDecayConfig cfg;
  cfg.m = 3.0;
  cfg.Ea_J_per_mol = 50892.977;
  cfg.A_per_h = 59.90229;
  cfg.sourced = true;
  return cfg;
}

ActivityDecayConfig kordabadi_hanken_industrial() {
  ActivityDecayConfig cfg;

  cfg.m = 5.0; // sourced, Kordabadi Eq. (4)
  cfg.Ea_J_per_mol = 47421.558; // interim, Fichtl CZA1; Hanken's Ed unpublished

  // Pre-exponential calibrated: Kd inverted from the closed form at Kordabadi's anchor (a = 0.40 at 1400 days, 245 C) 
  // then converted to A
  {
    const double t_anchor_h = presets::kIndustrialAnchorDays * 24.0;
    const double a = presets::kIndustrialAnchorActivity;
    const double T = presets::kIndustrialAnchorTempK;
    const double Kd_anchor  =
        (std::pow(a, 1.0 - cfg.m) - 1.0) / ((cfg.m - 1.0) * t_anchor_h);
    cfg.A_per_h = Kd_anchor * std::exp(cfg.Ea_J_per_mol / (units::R * T));
  }

  cfg.sourced = false;
  cfg.source =
      "Form sourced: Kordabadi & Jahanmiri (2007) Chem Eng Process 46, "
      "1299-1309, Eq. (4), citing L. Hanken MSc thesis NTNU 1995. Rate "
      "calibrated to their a = 0.40 at 1400 days anchor. Ed interim from "
      "Fichtl CZA1.";
  return cfg;
}

}

}

