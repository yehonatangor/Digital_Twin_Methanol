// Catalyst deactivation, Fichtl et al. (2015) Applied Catalysis A 502, 262-270

#include "degradation/activity_decay.hpp"
#include "_harness.inc"

int main() {
  std::printf("=== catalyst activity decay ===\n\n");

  const auto cza1 = activity_decay::presets::fichtl_cza1_523_553K();

  std::printf("[1] The fit is a 2-point Arrhenius through Fichtl's m = 3 points\n");
  check("reaction order m", cza1.m, 3.0, 1e-12);
  check("activation energy, kJ/mol", cza1.Ea_J_per_mol / 1000.0, 47.422, 0.01);
  check("pre-exponential, 1/h", cza1.A_per_h, 78.978, 0.01);
  checkTrue("declared sourced", cza1.sourced);
  checkTrue("the water multiplier is flagged unsourced", !cza1.water_multiplier_sourced);

  std::printf("\n[2] Kd follows Arrhenius\n");
  const double kd523 = activity_decay::Kd_per_h(523.0, cza1);
  const double kd553 = activity_decay::Kd_per_h(553.0, cza1);
  checkTrue("Kd is positive", kd523 > 0.0);
  checkTrue("Kd rises with temperature", kd553 > kd523);
  // ln(Kd2/Kd1) = -Ea/R (1/T2 - 1/T1)
  check("Arrhenius slope", std::log(kd553 / kd523),
        -cza1.Ea_J_per_mol / 8.314462618 * (1.0 / 553.0 - 1.0 / 523.0), 1e-9);

  std::printf("\n[3] The closed form solves da/dt = -Kd a^m for m = 3\n");
  // a(t) = a0 [1 + (m-1) Kd a0^(m-1) t]^(-1/(m-1))
  const double a0 = 1.0, t = 500.0;
  const double closed = activity_decay::activity_closed_form(a0, t, kd523, 3.0);
  const double expect = a0 * std::pow(1.0 + 2.0 * kd523 * a0 * a0 * t, -0.5);
  checkRel("closed form", closed, expect, 1e-9);
  check("a(0) = a0", activity_decay::activity_closed_form(a0, 0.0, kd523, 3.0), a0, 1e-12);
  checkTrue("activity decreases with time", closed < a0);

  std::printf("\n[4] Numerical integration matches the closed form\n");
  const auto r = activity_decay::integrate_activity_isothermal(1.0, 500.0, 2000, 523.0, cza1);
  checkTrue("ok", r.ok);
  checkRel("a_final matches the analytic solution", r.a_final, expect, 1e-4);
  checkTrue("a profile was recorded", !r.profile.empty());
  checkTrue("the profile is monotonically decreasing",
            r.profile.front().a >= r.profile.back().a);

  std::printf("\n[5] Fichtl's own reported loss over the fitted window\n");
  // Fichtl reports about 58 % loss of activity by roughly 1630 h at 523 K
  const auto long_run = activity_decay::integrate_activity_isothermal(1.0, 1600.0, 4000, 523.0, cza1);
  checkTrue("ok", long_run.ok);
  std::printf("       [INFO] a(1600 h) = %.4f, i.e. %.1f %% lost\n",
              long_run.a_final, 100.0 * (1.0 - long_run.a_final));
  check("activity at 1600 h", long_run.a_final, 0.42, 0.03);

  std::printf("\n[6] The other two Fichtl catalysts\n");
  const auto cza2 = activity_decay::presets::fichtl_cza2_523_553K();
  const auto cza3 = activity_decay::presets::fichtl_cza3_523_553K();
  check("CZA2 Ea, kJ/mol", cza2.Ea_J_per_mol / 1000.0, 39.009, 0.01);
  check("CZA2 A, 1/h",     cza2.A_per_h,               10.544, 0.01);
  check("CZA3 Ea, kJ/mol", cza3.Ea_J_per_mol / 1000.0, 50.893, 0.01);
  check("CZA3 A, 1/h",     cza3.A_per_h,               59.902, 0.01);

  std::printf("\n[7] The industrial anchor is flagged as awaiting a source\n");
  const auto ind = activity_decay::presets::kordabadi_hanken_industrial();
  check("anchor activity", activity_decay::presets::kIndustrialAnchorActivity, 0.40, 1e-12);
  check("anchor days",     activity_decay::presets::kIndustrialAnchorDays, 1400.0, 1e-12);
  check("anchor m",        ind.m, 5.0, 1e-12);
  checkTrue("Ed is flagged unsourced", !activity_decay::presets::kIndustrialEdSourced);

  std::printf("\n[8] scale_rate is a plain multiplier\n");
  check("half activity halves the rate", activity_decay::scale_rate(10.0, 0.5), 5.0, 1e-12);
  check("fresh leaves it alone",         activity_decay::scale_rate(10.0, 1.0), 10.0, 1e-12);

  return report("activity decay");
}
