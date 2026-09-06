#include "heat_exchanger.hpp"

#include <algorithm>
#include <cmath>

#include "flash.hpp"
#include "reactor/energy_balance.hpp"
#include "thermo.hpp"
#include "units.hpp"

namespace front_end {

namespace {

double streamEnthalpy_W(const std::unordered_map<Species, double>& F, double T_K) {
  double h = 0.0;
  for (const auto& [sp, n] : F) if (n > 0.0) h += n * thermo::enthalpy(sp, T_K);
  return h;
}

double streamCp_W_per_K(const std::unordered_map<Species, double>& F, double T_K) {
  double c = 0.0;
  for (const auto& [sp, n] : F) if (n > 0.0) c += n * thermo::cp(sp, T_K);
  return c;
}

// dH_vap(T) = R T^2 d(ln Psat)/dT, J/mol, from the same DIPPR-101 fit the
// flash uses to decide what condenses. Returns 0 for species with no vapour
// pressure data, which do not condense here
double dH_vap_J_per_mol(Species sp, double T_K) {
  if (!vaporPressureParams(sp)) return 0.0;
  const double h = std::max(1e-3, 1e-5 * T_K);   // small relative step
  const double p_hi = vaporPressure(sp, T_K + h);
  const double p_lo = vaporPressure(sp, T_K - h);
  if (!(p_hi > 0.0) || !(p_lo > 0.0)) return 0.0;
  const double dlnP_dT = (std::log(p_hi) - std::log(p_lo)) / (2.0 * h);
  return units::R * T_K * T_K * dlnP_dT;
}

}  // namespace

HXResult heat_to_temperature(const Stream& in, double T_target_K) {
  HXResult r;
  if (!(T_target_K > 0.0)) {
    r.message = "T_target_K must be positive";
    return r;
  }
  if (!(in.totalMolarFlow() > 0.0)) {
    r.outlet = in;
    r.outlet.temperature = T_target_K;
    r.ok = true;
    r.message = "ok (zero flow)";
    return r;
  }

  const double Q_W = streamEnthalpy_W(in.molar_flow, T_target_K) -
                     streamEnthalpy_W(in.molar_flow, in.temperature);

  r.outlet = in;
  r.outlet.temperature = T_target_K;
  r.duty_MW = Q_W / 1.0e6;
  r.ok = true;
  r.message = "ok";
  return r;
}

HXResult heat_with_duty(const Stream& in, double duty_MW, const HXConfig& cfg) {
  HXResult r;
  if (!(in.totalMolarFlow() > 0.0)) {
    r.outlet = in;
    r.ok = true;
    r.message = "ok (zero flow)";
    return r;
  }

  const double target_H_W = streamEnthalpy_W(in.molar_flow, in.temperature) + duty_MW * 1.0e6;

  // Newton on f(T) = H(F,T) - target_H, with stream heat capacity as f'(T)
  double T = in.temperature;
  bool converged = false;
  for (int it = 0; it < cfg.max_iter; ++it) {
    const double f  = streamEnthalpy_W(in.molar_flow, T) - target_H_W;
    const double df = streamCp_W_per_K(in.molar_flow, T);
    if (!(std::fabs(df) > 0.0)) break;
    double T_next = std::clamp(T - f / df, cfg.T_floor_K, cfg.T_ceiling_K);
    if (std::fabs(T_next - T) < cfg.T_tol_K) { T = T_next; converged = true; break; }
    T = T_next;
  }

  r.outlet = in;
  r.outlet.temperature = T;
  r.duty_MW = duty_MW;
  r.ok = converged;
  r.message = converged ? "ok" : "Newton did not converge on an outlet temperature";
  return r;
}

CondensingCoolResult cool_with_condensation(const Stream& in, double T_target_K, double P_Pa) {
  CondensingCoolResult r;

  if (!(T_target_K > 0.0)) {
    r.message = "T_target_K must be positive";
    return r;
  }
  const double P = (P_Pa > 0.0) ? P_Pa : in.pressure;
  if (!(P > 0.0)) {
    r.message = "no usable pressure: pass P_Pa or set the inlet stream's pressure";
    return r;
  }
  if (!(in.totalMolarFlow() > 0.0)) {
    r.ok = true;
    r.message = "ok (zero flow)";
    return r;
  }

  flash::FlashResult f;
  try {
    f = flash::solve(in, T_target_K, P);
  } catch (const std::exception& e) {
    r.message = std::string("flash::solve failed: ") + e.what();
    return r;
  }

  r.vapor_outlet  = f.vapor;
  r.liquid_outlet = f.liquid;
  r.vapor_fraction = f.vapor_fraction;
  r.flash_converged = f.converged;
  r.ideal_binary_pairs = f.ideal_binary_pairs;

  // Sensible: every mole, vapour or liquid, changes temperature
  const double sensible_W = streamEnthalpy_W(in.molar_flow, T_target_K) -
                            streamEnthalpy_W(in.molar_flow, in.temperature);

  // Latent: whatever condensed gave up its enthalpy of vaporisation
  double latent_W = 0.0;
  for (const auto& [sp, n] : f.liquid.molar_flow) {
    if (n > 0.0) latent_W += n * dH_vap_J_per_mol(sp, T_target_K);
  }
  r.condensed = latent_W > 0.0;

  const double Q_W = sensible_W - latent_W;
  r.duty_MW = Q_W / 1.0e6;
  r.latent_fraction = (std::fabs(Q_W) > 0.0) ? std::fabs(latent_W) / std::fabs(Q_W) : 0.0;

  r.ok = true;
  r.message = f.converged ? "ok" : "ok (flash did not converge; duty is from the last iterate)";
  return r;
}

TwoStreamHXResult exchange_two_streams(const Stream& hot_in, const Stream& cold_in,
                                        double UA_W_per_K, FlowArrangement arrangement,
                                        const HXConfig& /*cfg*/) {
  TwoStreamHXResult r;

  if (!(UA_W_per_K > 0.0)) {
    r.message = "UA_W_per_K must be positive (no default is provided -- see header note)";
    return r;
  }
  if (!(hot_in.totalMolarFlow() > 0.0) || !(cold_in.totalMolarFlow() > 0.0)) {
    r.message = "both streams must carry flow";
    return r;
  }

  // Effectiveness-NTU assumes constant heat capacity rates. Each stream's C is
  // evaluated at its own inlet temperature and held fixed across the exchanger
  const double C_hot  = streamCp_W_per_K(hot_in.molar_flow,  hot_in.temperature);
  const double C_cold = streamCp_W_per_K(cold_in.molar_flow, cold_in.temperature);
  if (!(C_hot > 0.0) || !(C_cold > 0.0)) {
    r.message = "degenerate heat capacity rate";
    return r;
  }

  r.C_min_W_per_K = std::min(C_hot, C_cold);
  r.C_max_W_per_K = std::max(C_hot, C_cold);
  const double Cr = r.C_min_W_per_K / r.C_max_W_per_K;
  r.NTU = UA_W_per_K / r.C_min_W_per_K;

  if (arrangement == FlowArrangement::Counterflow) {
    if (std::fabs(1.0 - Cr) < 1e-12) {
      r.effectiveness = r.NTU / (1.0 + r.NTU);
    } else {
      const double e = std::exp(-r.NTU * (1.0 - Cr));
      r.effectiveness = (1.0 - e) / (1.0 - Cr * e);
    }
  } else {
    r.effectiveness = (1.0 - std::exp(-r.NTU * (1.0 + Cr))) / (1.0 + Cr);
  }

  const double dT_in = hot_in.temperature - cold_in.temperature;
  const double Q_W = r.effectiveness * r.C_min_W_per_K * dT_in;
  r.duty_MW = Q_W / 1.0e6;

  r.hot_outlet  = hot_in;
  r.cold_outlet = cold_in;
  r.hot_outlet.temperature  = hot_in.temperature  - Q_W / C_hot;
  r.cold_outlet.temperature = cold_in.temperature + Q_W / C_cold;

  r.ok = true;
  r.message = "ok";
  return r;
}

namespace presets {

// Van-Dal quotes two different reactor temperatures and they are not
// interchangeable. Sec. 2.3.1 heats the industrial feed to 210 C in HX4;
// Table A.3's single-tube laboratory validation case runs at 220 C. Both are
// named explicitly so a caller cannot pick one by accident
double van_dal_industrial_reactor_inlet_T_K() { return units::celsiusToKelvin(210.0); }
double van_dal_lab_reactor_T_K() { return units::celsiusToKelvin(220.0); }

}  // namespace presets

}  // namespace front_end
