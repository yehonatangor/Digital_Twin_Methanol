#include "reactor_core.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "eos.hpp"
#include "lhhw.hpp"
#include "units.hpp"

namespace reactor {

// -----------------------------------------------------------------------------
// ReactorState
// -----------------------------------------------------------------------------
double ReactorState::total_flow_mol_s() const {
  double s = 0.0;
  for (double f : F) if (f > 0.0) s += f;
  return s;
}

SpeciesArray ReactorState::mole_fractions() const {
  SpeciesArray y{};
  const double tot = total_flow_mol_s();
  if (tot <= 0.0) return y;
  for (int i = 0; i < NS; ++i) {
    const std::size_t k = static_cast<std::size_t>(i);
    y[k] = (F[k] > 0.0) ? F[k] / tot : 0.0;
  }
  return y;
}

double ReactorState::mass_flow_kg_s() const {
  double m = 0.0;
  for (int i = 0; i < NS; ++i) {
    const std::size_t k = static_cast<std::size_t>(i);
    if (F[k] > 0.0) m += F[k] * molarMassKgPerMol(static_cast<Species>(i));
  }
  return m;
}

double ReactorState::mean_molar_mass_kg_mol() const {
  const double tot = total_flow_mol_s();
  return (tot > 0.0) ? mass_flow_kg_s() / tot : 0.0;
}

Stream ReactorState::toStream() const {
  Stream s;
  s.temperature = T_K;
  s.pressure    = P_Pa;
  s.phase       = Phase::Vapor;
  for (int i = 0; i < NS; ++i) {
    const std::size_t k = static_cast<std::size_t>(i);
    if (F[k] > 0.0) s.molar_flow[static_cast<Species>(i)] = F[k];
  }
  return s;
}

ReactorState ReactorState::fromStream(const Stream& s, double W_kg) {
  ReactorState st;
  st.T_K  = s.temperature;
  st.P_Pa = s.pressure;
  st.W_kg = W_kg;
  for (const auto& [sp, n] : s.molar_flow) {
    if (n > 0.0) st.F[idx(sp)] = n;
  }
  return st;
}

// -----------------------------------------------------------------------------
// Kinetics adapter. The one place Pa becomes bar
// -----------------------------------------------------------------------------
std::array<double, static_cast<std::size_t>(N_RXN)>
reaction_rates(const SpeciesArray& y, double T_K, double P_Pa) {
  const double P_bar = units::paToBar(P_Pa);
  const double p_CO2   = y[idx(Species::CO2)]   * P_bar;
  const double p_H2    = y[idx(Species::H2)]    * P_bar;
  const double p_CO    = y[idx(Species::CO)]    * P_bar;
  const double p_H2O   = y[idx(Species::H2O)]   * P_bar;
  const double p_CH3OH = y[idx(Species::CH3OH)] * P_bar;

  std::array<double, static_cast<std::size_t>(N_RXN)> r{};
  r[R_MEOH] = lhhw::r_CH3OH(p_CO2, p_H2, p_H2O, p_CH3OH, T_K);
  r[R_RWGS] = lhhw::r_RWGS(p_CO2, p_H2, p_H2O, p_CO, T_K);
  return r;
}

// -----------------------------------------------------------------------------
// Derived quantities
// -----------------------------------------------------------------------------
double axial_position_m(double W_kg, const BedGeometry& bed) {
  const double denom = bulk_density_kg_m3(bed) * cross_section_area_m2(bed);
  return (denom > 0.0) ? W_kg / denom : 0.0;
}

double molar_density_mol_m3(const ReactorState& s, bool use_real_gas_Z) {
  if (!(s.T_K > 0.0) || !(s.P_Pa > 0.0)) return 0.0;
  double Z = 1.0;
  if (use_real_gas_Z) {
    try {
      const Stream str = s.toStream();
      const auto mix = eos::mixtureParams(str, s.T_K);
      Z = eos::compressibilityFactor(mix, s.T_K, s.P_Pa, eos::RootSelect::Vapor);
    } catch (const std::exception&) {
      Z = 1.0;   // fall back to ideal rather than abort the integration
    }
  }
  if (!(Z > 0.0)) Z = 1.0;
  return s.P_Pa / (Z * units::R * s.T_K);
}

double mass_density_kg_m3(const ReactorState& s, bool use_real_gas_Z) {
  return molar_density_mol_m3(s, use_real_gas_Z) * s.mean_molar_mass_kg_mol();
}

double mass_flux_kg_m2s(const ReactorState& s, const BedGeometry& bed) {
  const double A = cross_section_area_m2(bed);
  return (A > 0.0) ? s.mass_flow_kg_s() / A : 0.0;
}

// -----------------------------------------------------------------------------
// RHS of the coupled system. st[0..NS-1] = F_i, st[NS] = T, st[NS+1] = P
// -----------------------------------------------------------------------------
namespace {

constexpr std::size_t kNVar = static_cast<std::size_t>(NS) + 2;
using Vec = std::array<double, kNVar>;

Vec toVec(const ReactorState& s) {
  Vec v{};
  for (int i = 0; i < NS; ++i) v[static_cast<std::size_t>(i)] = s.F[static_cast<std::size_t>(i)];
  v[static_cast<std::size_t>(NS)]     = s.T_K;
  v[static_cast<std::size_t>(NS) + 1] = s.P_Pa;
  return v;
}

ReactorState fromVec(const Vec& v, double W) {
  ReactorState s;
  for (int i = 0; i < NS; ++i) s.F[static_cast<std::size_t>(i)] = v[static_cast<std::size_t>(i)];
  s.T_K  = v[static_cast<std::size_t>(NS)];
  s.P_Pa = v[static_cast<std::size_t>(NS) + 1];
  s.W_kg = W;
  return s;
}

bool rhs(const Vec& v, const ReactorConfig& cfg, Vec& out) {
  const ReactorState s = fromVec(v, 0.0);
  if (!(s.T_K > 0.0) || !(s.P_Pa > 0.0)) return false;
  if (!(s.total_flow_mol_s() > 0.0)) return false;

  const SpeciesArray y = s.mole_fractions();
  const auto r = reaction_rates(y, s.T_K, s.P_Pa);
  for (double x : r) if (!std::isfinite(x)) return false;

  // Mole balance: dF_i/dW = sum_j nu_ij r_j
  for (int i = 0; i < NS; ++i) {
    double d = 0.0;
    for (int j = 0; j < N_RXN; ++j) {
      d += kStoich[static_cast<std::size_t>(j)][static_cast<std::size_t>(i)] *
           r[static_cast<std::size_t>(j)];
    }
    out[static_cast<std::size_t>(i)] = d;
  }

  // Energy balance
  out[static_cast<std::size_t>(NS)] =
      dTdW_K_per_kg(s.F, s.T_K, r, cfg.thermal, cfg.cooling, cfg.bed);

  // Pressure drop
  double dPdW = 0.0;
  if (cfg.dp.model != PressureDropModel::None) {
    const double rho = mass_density_kg_m3(s, cfg.use_real_gas_Z);
    const double G   = mass_flux_kg_m2s(s, cfg.bed);
    const double mu  = mixture_viscosity_Pa_s(y, s.T_K, cfg.visc);
    if (!(rho > 0.0) || !(mu > 0.0)) return false;
    dPdW = pressure_gradient_dPdW(cfg.dp, mu, rho, G, cfg.bed);
  }
  out[static_cast<std::size_t>(NS) + 1] = dPdW;

  for (double x : out) if (!std::isfinite(x)) return false;
  return true;
}

}  // namespace

double ReactorResult::co2_conversion(const ReactorState& inlet) const {
  const double in  = inlet.F[idx(Species::CO2)];
  const double out = outlet.F[idx(Species::CO2)];
  return (in > 0.0) ? (in - out) / in : 0.0;
}

double ReactorResult::meoh_yield_mol_s() const { return outlet.F[idx(Species::CH3OH)]; }

ReactorResult integrate_reactor(const ReactorState& inlet, const ReactorConfig& cfg) {
  ReactorResult res;

  const double W_end = catalyst_mass_per_tube_kg(cfg.bed);
  if (!(W_end > 0.0)) {
    res.message = "invalid bed geometry: zero catalyst mass per tube";
    return res;
  }
  if (!(inlet.T_K > 0.0) || !(inlet.P_Pa > 0.0) || !(inlet.total_flow_mol_s() > 0.0)) {
    res.message = "inlet state must have positive T, P and total flow";
    return res;
  }

  // n_steps floors the count, max_step_kg_cat caps the size
  int n = cfg.n_steps > 0 ? cfg.n_steps : 1;
  if (cfg.max_step_kg_cat > 0.0) {
    const double need = std::ceil(W_end / cfg.max_step_kg_cat);
    // Guard against a non-terminating integration
    const double capped = std::min(need, 5.0e6);
    n = std::max(n, static_cast<int>(capped));
  }
  res.n_steps_used = n;
  const double h = W_end / static_cast<double>(n);
  res.step_kg_cat = h;

  // Inlet flows, kept for the clamp's relative-excursion test below
  SpeciesArray F_in = inlet.F;
  double F_in_max = 0.0;
  for (double f : F_in) F_in_max = std::max(F_in_max, f);
  if (!(F_in_max > 0.0)) F_in_max = 1.0;

  Vec v = toVec(inlet);
  double W = 0.0;

  if (cfg.record_profile) {
    res.profile.reserve(static_cast<std::size_t>(n) + 1);
    res.profile.push_back(fromVec(v, W));
  }

  Vec k1{}, k2{}, k3{}, k4{}, tmp{};
  for (int step = 0; step < n; ++step) {
    if (!rhs(v, cfg, k1)) { res.message = "rhs failed (k1)."; res.outlet = fromVec(v, W); return res; }
    for (std::size_t i = 0; i < kNVar; ++i) tmp[i] = v[i] + 0.5 * h * k1[i];
    if (!rhs(tmp, cfg, k2)) { res.message = "rhs failed (k2)."; res.outlet = fromVec(v, W); return res; }
    for (std::size_t i = 0; i < kNVar; ++i) tmp[i] = v[i] + 0.5 * h * k2[i];
    if (!rhs(tmp, cfg, k3)) { res.message = "rhs failed (k3)."; res.outlet = fromVec(v, W); return res; }
    for (std::size_t i = 0; i < kNVar; ++i) tmp[i] = v[i] + h * k3[i];
    if (!rhs(tmp, cfg, k4)) { res.message = "rhs failed (k4)."; res.outlet = fromVec(v, W); return res; }

    for (std::size_t i = 0; i < kNVar; ++i) {
      v[i] += (h / 6.0) * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
    }
    W += h;

    // Clamp negative flows at zero, but score every clamp against inlet flow:
    // the clamp only ever adds material, so a large one fabricates moles
    for (int i = 0; i < NS; ++i) {
      const std::size_t k = static_cast<std::size_t>(i);
      if (v[k] < 0.0) {
        const double rel = -v[k] / F_in_max;
        res.max_clamp_rel = std::max(res.max_clamp_rel, rel);
        v[k] = 0.0;
      }
    }
    if (res.max_clamp_rel > cfg.max_clamp_rel) {
      res.outlet = fromVec(v, W);
      res.message = "non-negativity clamp absorbed a physically significant " +
                    std::to_string(res.max_clamp_rel) +
                    " of inlet flow, above the tolerance " + std::to_string(cfg.max_clamp_rel) +
                    " -- the step is too large and the result would rest on fabricated moles";
      return res;
    }

    const double T = v[static_cast<std::size_t>(NS)];
    const double P = v[static_cast<std::size_t>(NS) + 1];
    if (!(T > cfg.T_min_K) || !(T < cfg.T_max_K)) {
      res.outlet = fromVec(v, W);
      res.message = "temperature left the configured bounds at W = " + std::to_string(W);
      return res;
    }
    if (!(P > cfg.P_min_Pa)) {
      res.outlet = fromVec(v, W);
      res.message = "pressure fell below the configured minimum at W = " + std::to_string(W);
      return res;
    }

    if (cfg.record_profile) res.profile.push_back(fromVec(v, W));
  }

  res.outlet = fromVec(v, W);
  res.ok = true;
  res.message = "ok";
  return res;
}

ReactorState van_dal_lab_feed() {
  // Van-Dal Table A.3: 2.8e-5 kg/s, 50 bar, 220 C, molar composition
  // CO 4 %, H2 82 %, CO2 3 %, Ar 11 %
  ReactorState s;
  s.T_K  = units::celsiusToKelvin(220.0);
  s.P_Pa = units::barToPa(50.0);
  s.W_kg = 0.0;

  const double y_CO = 0.04, y_H2 = 0.82, y_CO2 = 0.03, y_Ar = 0.11;
  const double M_mean = y_CO  * molarMassKgPerMol(Species::CO)
                      + y_H2  * molarMassKgPerMol(Species::H2)
                      + y_CO2 * molarMassKgPerMol(Species::CO2)
                      + y_Ar  * molarMassKgPerMol(Species::Ar);
  const double m_dot = 2.8e-5;              // kg/s
  const double n_tot = m_dot / M_mean;      // mol/s

  s.F[idx(Species::CO)]  = y_CO  * n_tot;
  s.F[idx(Species::H2)]  = y_H2  * n_tot;
  s.F[idx(Species::CO2)] = y_CO2 * n_tot;
  s.F[idx(Species::Ar)]  = y_Ar  * n_tot;
  return s;
}

void write_profile_csv(const ReactorResult& res, const BedGeometry& bed, const std::string& path) {
  std::FILE* f = std::fopen(path.c_str(), "w");
  if (!f) return;
  std::fprintf(f, "W_kg,z_m,T_K,P_bar");
  for (int i = 0; i < NS; ++i) std::fprintf(f, ",y_%s", speciesName(static_cast<Species>(i)));
  std::fprintf(f, "\n");
  for (const auto& s : res.profile) {
    std::fprintf(f, "%.9g,%.9g,%.9g,%.9g", s.W_kg, axial_position_m(s.W_kg, bed), s.T_K,
                 units::paToBar(s.P_Pa));
    const SpeciesArray y = s.mole_fractions();
    for (int i = 0; i < NS; ++i) std::fprintf(f, ",%.9g", y[static_cast<std::size_t>(i)]);
    std::fprintf(f, "\n");
  }
  std::fclose(f);
}

}  // namespace reactor
