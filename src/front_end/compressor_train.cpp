#include "compressor_train.hpp"

#include <cmath>
#include "thermo.hpp"
#include "units.hpp"

namespace front_end {

CompressorResult run_compressor_stage(Species species, double m_dot_kg_s, double P_in_bar,
                                       double P_out_bar, const CompressorConfig& cfg) {
  CompressorResult r;

  if (m_dot_kg_s < 0.0) {
    r.ok = false;
    r.message = "m_dot_kg_s must be non-negative";
    return r;
  }
  if (P_in_bar <= 0.0 || P_out_bar <= 0.0) {
    r.ok = false;
    r.message = "pressures must be positive";
    return r;
  }
  if (P_out_bar < P_in_bar) {
    r.ok = false;
    r.message = "P_out_bar < P_in_bar -- not a compression (beta < 1)";
    return r;
  }

  r.beta = P_out_bar / P_in_bar;

  // Same formula as run_compressor(), with species as a parameter
  const double cp_molar_J_per_molK = thermo::cp(species, cfg.T_in_K);
  const double M_kg_per_mol = properties(species).molar_mass / 1000.0;
  r.cp_J_per_kgK = cp_molar_J_per_molK / M_kg_per_mol;

  const double cv_molar = cp_molar_J_per_molK - units::R;
  if (!(cv_molar > 0.0)) {
    r.ok = false;
    r.message = "degenerate Cv (Cp <= R) -- cannot form heat-capacity ratio k";
    return r;
  }
  r.k_isentropic_exponent = cp_molar_J_per_molK / cv_molar;

  if (cfg.eta_isentropic <= 0.0 || cfg.eta_isentropic > 1.0 ||
      cfg.eta_mechanical <= 0.0 || cfg.eta_mechanical > 1.0) {
    r.ok = false;
    r.message = "eta_isentropic and eta_mechanical must be in (0, 1]";
    return r;
  }

  const double exponent = (r.k_isentropic_exponent - 1.0) / r.k_isentropic_exponent;
  const double work_term = std::pow(r.beta, exponent) - 1.0;
  const double P_comp_W = m_dot_kg_s * r.cp_J_per_kgK * cfg.T_in_K /
                           (cfg.eta_isentropic * cfg.eta_mechanical) * work_term;

  r.P_comp_MW = P_comp_W / 1.0e6;
  r.P_cooling_MW = r.P_comp_MW;   // same stated Mucci Sec. 3.3 assumption: cooling duty = compression power

  if (cfg.capacity_per_unit_kg_s > 0.0 && cfg.n_units_parallel >= 1) {
    const double per_unit_flow = m_dot_kg_s / cfg.n_units_parallel;
    const double min_flow = cfg.min_load_fraction_single * cfg.capacity_per_unit_kg_s;
    r.below_min_turndown = per_unit_flow < min_flow;
  }

  r.ok = true;
  r.message = r.below_min_turndown
                  ? "ok (WARNING: below advisory minimum turndown -- see compressor.hpp header note)"
                  : "ok";
  return r;
}

CompressorResult run_compressor_stage_mixture(const Stream& gas, double P_in_bar, double P_out_bar,
                                               const CompressorConfig& cfg) {
  CompressorResult r;

  if (P_in_bar <= 0.0 || P_out_bar <= 0.0) {
    r.ok = false;
    r.message = "pressures must be positive";
    return r;
  }
  if (P_out_bar < P_in_bar) {
    r.ok = false;
    r.message = "P_out_bar < P_in_bar -- not a compression (beta < 1)";
    return r;
  }

  const double n_total = gas.totalMolarFlow();
  if (!(n_total > 0.0)) {
    // No flow: zero duty is the correct answer, not an error
    r.beta = P_out_bar / P_in_bar;
    r.ok = true;
    r.message = "ok (zero flow)";
    return r;
  }

  r.beta = P_out_bar / P_in_bar;

  // Mole-weighted mixture cp and molar mass at the compressor inlet
  double cp_molar_J_per_molK = 0.0;
  double M_kg_per_mol = 0.0;
  double m_dot_kg_s = 0.0;
  for (const auto& [sp, n] : gas.molar_flow) {
    if (n <= 0.0) continue;
    const double y = n / n_total;
    cp_molar_J_per_molK += y * thermo::cp(sp, cfg.T_in_K);
    M_kg_per_mol += y * properties(sp).molar_mass / 1000.0;
    m_dot_kg_s += n * properties(sp).molar_mass / 1000.0;
  }
  if (!(M_kg_per_mol > 0.0)) {
    r.ok = false;
    r.message = "degenerate mixture molar mass";
    return r;
  }
  r.cp_J_per_kgK = cp_molar_J_per_molK / M_kg_per_mol;

  const double cv_molar = cp_molar_J_per_molK - units::R;
  if (!(cv_molar > 0.0)) {
    r.ok = false;
    r.message = "degenerate Cv (Cp <= R) -- cannot form heat-capacity ratio k";
    return r;
  }
  r.k_isentropic_exponent = cp_molar_J_per_molK / cv_molar;

  if (cfg.eta_isentropic <= 0.0 || cfg.eta_isentropic > 1.0 ||
      cfg.eta_mechanical <= 0.0 || cfg.eta_mechanical > 1.0) {
    r.ok = false;
    r.message = "eta_isentropic and eta_mechanical must be in (0, 1]";
    return r;
  }

  const double exponent = (r.k_isentropic_exponent - 1.0) / r.k_isentropic_exponent;
  const double work_term = std::pow(r.beta, exponent) - 1.0;
  const double P_comp_W = m_dot_kg_s * r.cp_J_per_kgK * cfg.T_in_K /
                           (cfg.eta_isentropic * cfg.eta_mechanical) * work_term;

  r.P_comp_MW = P_comp_W / 1.0e6;
  r.P_cooling_MW = r.P_comp_MW;

  if (cfg.capacity_per_unit_kg_s > 0.0 && cfg.n_units_parallel >= 1) {
    const double per_unit_flow = m_dot_kg_s / cfg.n_units_parallel;
    const double min_flow = cfg.min_load_fraction_single * cfg.capacity_per_unit_kg_s;
    r.below_min_turndown = per_unit_flow < min_flow;
  }

  r.ok = true;
  r.message = r.below_min_turndown
                  ? "ok (WARNING: below advisory minimum turndown)"
                  : "ok";
  return r;
}

CompressorTrainResult run_compressor_train(Species species, double m_dot_kg_s, double P_in_bar,
                                            double P_out_bar, const CompressorTrainConfig& cfg) {
  CompressorTrainResult res;

  if (cfg.n_stages < 1) {
    res.message = "CompressorTrainConfig::n_stages must be >= 1";
    return res;
  }
  if (P_in_bar <= 0.0 || P_out_bar <= 0.0) {
    res.message = "pressures must be positive";
    return res;
  }
  if (P_out_bar < P_in_bar) {
    res.message = "P_out_bar < P_in_bar -- not a compression";
    return res;
  }

  res.overall_beta = P_out_bar / P_in_bar;
  const double stage_beta = std::pow(res.overall_beta, 1.0 / static_cast<double>(cfg.n_stages));

  double stage_P_in_bar = P_in_bar;
  for (int i = 0; i < cfg.n_stages; ++i) {
    const double stage_P_out_bar = stage_P_in_bar * stage_beta;
    const CompressorResult stage =
        run_compressor_stage(species, m_dot_kg_s, stage_P_in_bar, stage_P_out_bar, cfg.stage_cfg);
    if (!stage.ok) {
      res.message = "stage " + std::to_string(i) + " failed: " + stage.message;
      return res;
    }
    res.stages.push_back(stage);
    res.total_P_comp_MW += stage.P_comp_MW;
    res.total_P_cooling_MW += stage.P_cooling_MW;
    stage_P_in_bar = stage_P_out_bar;   // intercooled back to cfg.stage_cfg.T_in_K -- next stage's run_compressor_stage() call uses cfg.T_in_K again, not the previous stage's outlet T
  }

  res.ok = true;
  res.message = "ok (" + std::to_string(cfg.n_stages) + " stage(s), overall beta = " +
                std::to_string(res.overall_beta) + ")";
  return res;
}

}  // namespace front_end

