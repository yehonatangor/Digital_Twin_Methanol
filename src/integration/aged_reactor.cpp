#include "aged_reactor.hpp"

#include <algorithm>
#include <cmath>

#include "units.hpp"

namespace integration {

namespace {

// Same structure as reactor_core.cpp's rhs(), with reaction_rates() scaled
// by activity. Every physics call is the same public function
constexpr std::size_t kNVar = static_cast<std::size_t>(reactor::NS) + 2;
using Vec = std::array<double, kNVar>;

Vec toVec(const reactor::ReactorState& s) {
  Vec v{};
  for (int i = 0; i < reactor::NS; ++i) v[static_cast<std::size_t>(i)] = s.F[static_cast<std::size_t>(i)];
  v[static_cast<std::size_t>(reactor::NS)]     = s.T_K;
  v[static_cast<std::size_t>(reactor::NS) + 1] = s.P_Pa;
  return v;
}

reactor::ReactorState fromVec(const Vec& v, double W) {
  reactor::ReactorState s;
  for (int i = 0; i < reactor::NS; ++i) s.F[static_cast<std::size_t>(i)] = v[static_cast<std::size_t>(i)];
  s.T_K  = v[static_cast<std::size_t>(reactor::NS)];
  s.P_Pa = v[static_cast<std::size_t>(reactor::NS) + 1];
  s.W_kg = W;
  return s;
}

bool aged_rhs(const Vec& v, const reactor::ReactorConfig& cfg, double activity, Vec& out) {
  const reactor::ReactorState s = fromVec(v, 0.0);
  if (!(s.T_K > 0.0) || !(s.P_Pa > 0.0) || !(s.total_flow_mol_s() > 0.0)) return false;

  const reactor::SpeciesArray y = s.mole_fractions();
  auto r = reactor::reaction_rates(y, s.T_K, s.P_Pa);
  // The one coupling point: scale the raw LHHW rate by activity
  for (double& x : r) x = activity_decay::scale_rate(x, activity);
  for (double x : r) if (!std::isfinite(x)) return false;

  for (int i = 0; i < reactor::NS; ++i) {
    double d = 0.0;
    for (int j = 0; j < reactor::N_RXN; ++j) {
      d += reactor::kStoich[static_cast<std::size_t>(j)][static_cast<std::size_t>(i)] *
           r[static_cast<std::size_t>(j)];
    }
    out[static_cast<std::size_t>(i)] = d;
  }

  out[static_cast<std::size_t>(reactor::NS)] =
      reactor::dTdW_K_per_kg(s.F, s.T_K, r, cfg.thermal, cfg.cooling, cfg.bed);

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

reactor::ReactorResult integrate_aged_reactor(const reactor::ReactorState& inlet,
                                               const reactor::ReactorConfig& cfg,
                                               double activity) {
  reactor::ReactorResult res;

  if (!(activity > 0.0) || activity > 1.0) {
    res.message = "activity must be in (0, 1]";
    return res;
  }

  const double W_end = reactor::catalyst_mass_per_tube_kg(cfg.bed);
  if (!(W_end > 0.0)) {
    res.message = "invalid bed geometry: zero catalyst mass per tube";
    return res;
  }
  if (!(inlet.T_K > 0.0) || !(inlet.P_Pa > 0.0) || !(inlet.total_flow_mol_s() > 0.0)) {
    res.message = "inlet state must have positive T, P and total flow";
    return res;
  }

  // Grid selection must stay identical to reactor_core.cpp's. This function
  // is a deliberate near-copy of integrate_reactor(), so any change there has
  // to be made here too. The bit-for-bit activity = 1.0 test is what enforces
  // that
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
    if (!aged_rhs(v, cfg, activity, k1)) { res.message = "rhs failed (k1)."; res.outlet = fromVec(v, W); return res; }
    for (std::size_t i = 0; i < kNVar; ++i) tmp[i] = v[i] + 0.5 * h * k1[i];
    if (!aged_rhs(tmp, cfg, activity, k2)) { res.message = "rhs failed (k2)."; res.outlet = fromVec(v, W); return res; }
    for (std::size_t i = 0; i < kNVar; ++i) tmp[i] = v[i] + 0.5 * h * k2[i];
    if (!aged_rhs(tmp, cfg, activity, k3)) { res.message = "rhs failed (k3)."; res.outlet = fromVec(v, W); return res; }
    for (std::size_t i = 0; i < kNVar; ++i) tmp[i] = v[i] + h * k3[i];
    if (!aged_rhs(tmp, cfg, activity, k4)) { res.message = "rhs failed (k4)."; res.outlet = fromVec(v, W); return res; }

    for (std::size_t i = 0; i < kNVar; ++i) v[i] += (h / 6.0) * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
    W += h;

    // Measured clamp, same contract as reactor_core.cpp
    for (int i = 0; i < reactor::NS; ++i) {
      const std::size_t k = static_cast<std::size_t>(i);
      if (v[k] < 0.0) {
        res.max_clamp_rel = std::max(res.max_clamp_rel, -v[k] / F_in_max);
        v[k] = 0.0;
      }
    }
    if (res.max_clamp_rel > cfg.max_clamp_rel) {
      res.outlet = fromVec(v, W);
      res.message = "non-negativity clamp absorbed a physically significant amount";
      return res;
    }

    const double T = v[static_cast<std::size_t>(reactor::NS)];
    const double P = v[static_cast<std::size_t>(reactor::NS) + 1];
    if (!(T > cfg.T_min_K) || !(T < cfg.T_max_K)) {
      res.outlet = fromVec(v, W);
      res.message = "temperature left the configured bounds";
      return res;
    }
    if (!(P > cfg.P_min_Pa)) {
      res.outlet = fromVec(v, W);
      res.message = "pressure fell below the configured minimum";
      return res;
    }

    if (cfg.record_profile) res.profile.push_back(fromVec(v, W));
  }

  res.outlet = fromVec(v, W);
  res.ok = true;
  res.message = "ok";
  return res;
}

AgingCampaignResult run_aging_campaign(const reactor::ReactorState& inlet,
                                        const reactor::ReactorConfig& reactor_cfg,
                                        const activity_decay::ActivityDecayConfig& decay_cfg,
                                        double aging_T_K, double t_end_h, int decay_steps,
                                        const std::vector<double>& checkpoint_times_h) {
  AgingCampaignResult res;

  if (checkpoint_times_h.empty()) {
    res.message = "checkpoint_times_h must be non-empty";
    return res;
  }
  if (!std::is_sorted(checkpoint_times_h.begin(), checkpoint_times_h.end())) {
    res.message = "checkpoint_times_h must be sorted ascending";
    return res;
  }
  if (checkpoint_times_h.back() > t_end_h) {
    res.message = "checkpoint times must not exceed t_end_h";
    return res;
  }

  res.decay = activity_decay::integrate_activity_isothermal(1.0, t_end_h, decay_steps,
                                                             aging_T_K, decay_cfg);
  if (!res.decay.ok) {
    res.message = "activity integration failed: " + res.decay.message;
    return res;
  }

  for (double t : checkpoint_times_h) {
    // First profile point at or after the checkpoint; the profile is ordered
    double a = res.decay.a_final;
    for (const auto& pt : res.decay.profile) {
      if (pt.t_h >= t) { a = pt.a; break; }
    }
    AgingCheckpoint cp;
    cp.t_h = t;
    cp.activity = a;
    cp.reactor = integrate_aged_reactor(inlet, reactor_cfg, a);
    res.checkpoints.push_back(std::move(cp));
  }

  res.ok = true;
  res.message = "ok";
  return res;
}

}  // namespace integration
