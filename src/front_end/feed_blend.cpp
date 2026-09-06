#include "feed_blend.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "reactor/energy_balance.hpp"
#include "thermo.hpp"

namespace front_end {

namespace {

double mol_s_of(const Stream& s, Species sp) {
  auto it = s.molar_flow.find(sp);
  return (it == s.molar_flow.end()) ? 0.0 : it->second;
}

// sum_i F_i * enthalpy_i(T), in W
double streamEnthalpy_W(const std::unordered_map<Species, double>& F, double T_K) {
  double h = 0.0;
  for (const auto& [sp, n] : F) {
    if (n > 0.0) h += n * thermo::enthalpy(sp, T_K);
  }
  return h;
}

double streamCp_W_per_K(const std::unordered_map<Species, double>& F, double T_K) {
  double c = 0.0;
  for (const auto& [sp, n] : F) {
    if (n > 0.0) c += n * thermo::cp(sp, T_K);
  }
  return c;
}

}  // namespace

double stoichiometric_number(const Stream& s) {
  const double h2  = mol_s_of(s, Species::H2);
  const double co  = mol_s_of(s, Species::CO);
  const double co2 = mol_s_of(s, Species::CO2);
  const double denom = co + co2;
  if (!(denom > 0.0)) {
    throw std::runtime_error(
        "stoichiometric_number: (CO + CO2) is zero -- SN is undefined for a stream "
        "with no carbon oxides");
  }
  return (h2 - co2) / denom;
}

Stream mix_streams(const std::vector<Stream>& inlets, const BlendConfig& cfg) {
  Stream out;

  std::unordered_map<Species, double> F;
  double H_in_W = 0.0;
  double flow_weighted_T = 0.0;
  double total_n = 0.0;
  double P_min = 0.0;
  bool have_P = false;

  for (const auto& s : inlets) {
    const double n_s = s.totalMolarFlow();
    if (n_s <= 0.0) continue;
    for (const auto& [sp, n] : s.molar_flow) {
      if (n > 0.0) F[sp] += n;
    }
    H_in_W += streamEnthalpy_W(s.molar_flow, s.temperature);
    flow_weighted_T += n_s * s.temperature;
    total_n += n_s;
    // A passive header cannot raise pressure above its lowest inlet
    if (s.pressure > 0.0) {
      P_min = have_P ? std::min(P_min, s.pressure) : s.pressure;
      have_P = true;
    }
  }

  if (total_n <= 0.0) {
    return out;   // nothing to balance; empty stream rather than a divide by zero
  }

  out.molar_flow = F;
  out.pressure = have_P ? P_min : 0.0;
  out.phase = Phase::Vapor;

  // Newton on f(T) = H(F, T) - H_in, with stream heat capacity as f'(T)
  // Initial guess is the flow-weighted inlet temperature
  double T = flow_weighted_T / total_n;
  T = std::clamp(T, cfg.T_floor_K, cfg.T_ceiling_K);

  for (int it = 0; it < cfg.max_iter; ++it) {
    const double f  = streamEnthalpy_W(F, T) - H_in_W;
    const double df = streamCp_W_per_K(F, T);
    if (!(std::fabs(df) > 0.0)) break;
    double T_next = T - f / df;
    T_next = std::clamp(T_next, cfg.T_floor_K, cfg.T_ceiling_K);
    const bool done = std::fabs(T_next - T) < cfg.T_tol_K;
    T = T_next;
    if (done) break;
  }

  out.temperature = T;
  return out;
}

BlendResult blend_to_target_SN(const Stream& syngas, const Stream& h2_stream,
                                const BlendConfig& cfg) {
  BlendResult r;

  const double co  = mol_s_of(syngas, Species::CO);
  const double co2 = mol_s_of(syngas, Species::CO2);
  const double h2  = mol_s_of(syngas, Species::H2);

  if (!(co + co2 > 0.0)) {
    r.message = "syngas has no carbon oxides -- SN is undefined, nothing to blend against";
    return r;
  }

  r.SN_before_topup = (h2 - co2) / (co + co2);

  // x = target*(CO + CO2) - H2 + CO2
  double x = cfg.target_SN * (co + co2) - h2 + co2;

  const double h2_available = mol_s_of(h2_stream, Species::H2);
  bool shortfall = false;
  if (x <= 0.0) {
    x = 0.0;   // already at or above target; do not force-dilute back down
  } else if (x > h2_available) {
    x = h2_available;
    shortfall = true;
  }
  r.h2_topup_mol_s = x;

  // Scale the whole hydrogen stream by the fraction actually used, so any
  // non-H2 species it carries keeps its composition ratio
  Stream h2_used;
  h2_used.temperature = h2_stream.temperature;
  h2_used.pressure    = h2_stream.pressure;
  h2_used.phase       = h2_stream.phase;
  if (h2_available > 0.0 && x > 0.0) {
    const double frac = x / h2_available;
    for (const auto& [sp, n] : h2_stream.molar_flow) {
      if (n > 0.0) h2_used.molar_flow[sp] = n * frac;
    }
  }

  r.blended = mix_streams({syngas, h2_used}, cfg);

  const double co_b  = mol_s_of(r.blended, Species::CO);
  const double co2_b = mol_s_of(r.blended, Species::CO2);
  const double h2_b  = mol_s_of(r.blended, Species::H2);
  r.SN_after = (co_b + co2_b > 0.0) ? (h2_b - co2_b) / (co_b + co2_b) : 0.0;

  r.ok = !shortfall;
  r.message = shortfall
                  ? "hydrogen stream could not meet the target SN: only " +
                        std::to_string(h2_available) + " mol/s available, " +
                        std::to_string(cfg.target_SN * (co + co2) - h2 + co2) +
                        " mol/s required -- this is an electrolyser sizing question"
                  : "ok";
  return r;
}

}  // namespace front_end
