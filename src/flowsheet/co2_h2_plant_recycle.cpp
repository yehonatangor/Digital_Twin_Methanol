#include "co2_h2_plant_recycle.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "front_end/feed_blend.hpp"
#include "thermo.hpp"
#include "front_end/heat_exchanger.hpp"
#include "integration/aged_reactor.hpp"
#include "species.hpp"
#include "units.hpp"

namespace flowsheet {

namespace {

double species_flow(const Stream& s, Species sp) {
  const auto it = s.molar_flow.find(sp);
  return (it == s.molar_flow.end()) ? 0.0 : it->second;
}

// Largest relative change across the union of both streams' species
double max_relative_change(const Stream& a, const Stream& b) {
  double worst = 0.0;
  const double scale = std::max(a.totalMolarFlow(), b.totalMolarFlow());
  if (!(scale > 0.0)) return 0.0;
  for (int i = 0; i < static_cast<int>(Species::Count); ++i) {
    const Species sp = static_cast<Species>(i);
    const double x = species_flow(a, sp);
    const double y = species_flow(b, sp);
    worst = std::max(worst, std::abs(y - x) / scale);
  }
  return worst;
}

}  // namespace

namespace detail {

// Shared by the fresh and aged entry points. activity == 1.0 must reproduce
// integrate_reactor() bit for bit, which is what aged_reactor.hpp guarantees
RecycleLoopResult run_recycle_with_activity(double m_dot_CO2_fresh_kg_s,
                                             double m_dot_H2_fresh_kg_s,
                                             const RecycleLoopConfig& cfg, double activity) {
  RecycleLoopResult res;

  if (!(activity > 0.0) || activity > 1.0) {
    res.message = "activity must be in (0, 1]";
    return res;
  }

  if (m_dot_CO2_fresh_kg_s <= 0.0 || m_dot_H2_fresh_kg_s <= 0.0) {
    res.message = "fresh CO2 and H2 feeds must both be positive";
    return res;
  }
  if (!(cfg.tol_rel > 0.0) || cfg.max_iterations <= 0) {
    res.message = "tol_rel must be positive and max_iterations at least 1";
    return res;
  }
  if (!(cfg.relaxation > 0.0) || cfg.relaxation > 1.0) {
    res.message = "relaxation must be in (0, 1]";
    return res;
  }

  // Fresh feed, mixed once and held fixed
  const Stream co2_feed = co2_fresh_feed(m_dot_CO2_fresh_kg_s);
  const Stream h2_feed  = h2_fresh_feed(m_dot_H2_fresh_kg_s);
  res.fresh_feed = front_end::mix_streams({co2_feed, h2_feed});
  res.fresh_feed.pressure = units::barToPa(cfg.plant_cfg.reactor_inlet_P_bar);
  // The feed is delivered cold. Setting it to the reactor inlet temperature
  // here would silently zero the pre-heat duty, which is the single largest
  // heating load in the flowsheet

  reactor::ReactorConfig rcfg = cfg.plant_cfg.reactor_cfg;
  if (rcfg.bed.n_tubes <= 1) rcfg.bed = presets::van_dal_catalyst_shi_tubes();
  res.n_tubes_used = rcfg.bed.n_tubes;
  if (res.n_tubes_used <= 0) {
    res.message = "bed geometry gives a non-positive tube count";
    return res;
  }

  // Tear stream: the recycle. Start from nothing and substitute
  Stream tear;                       // x_n
  tear.temperature = cfg.plant_cfg.knockout_T_K;
  tear.pressure    = units::barToPa(cfg.plant_cfg.reactor_inlet_P_bar);

  Stream prev_tear, prev_g;          // x_{n-1}, g_{n-1}
  bool have_previous = false;

  const double M_meoh = properties(Species::CH3OH).molar_mass;

  for (int iter = 0; iter < cfg.max_iterations; ++iter) {
    // ---- one pass of the loop map g(x) ------------------------------------
    Stream mixed = front_end::mix_streams({res.fresh_feed, tear});
    mixed.pressure = units::barToPa(cfg.plant_cfg.reactor_inlet_P_bar);

    const front_end::HXResult heated =
        front_end::heat_to_temperature(mixed, cfg.plant_cfg.reactor_inlet_T_K);
    if (!heated.ok) {
      res.message = "reactor pre-heat failed: " + heated.message;
      return res;
    }
    res.feed_preheat_duty_MW = heated.duty_MW;
    res.converged_reactor_inlet = heated.outlet;
    res.converged_reactor_inlet.pressure = units::barToPa(cfg.plant_cfg.reactor_inlet_P_bar);

    Stream per_tube = res.converged_reactor_inlet;
    for (auto& kv : per_tube.molar_flow) kv.second /= static_cast<double>(res.n_tubes_used);

    const reactor::ReactorState inlet = reactor::ReactorState::fromStream(per_tube);
    res.reactor = (activity >= 1.0)
                      ? reactor::integrate_reactor(inlet, rcfg)
                      : integration::integrate_aged_reactor(inlet, rcfg, activity);
    if (!res.reactor.ok) {
      res.message = "reactor integration failed: " + res.reactor.message;
      res.n_iterations = iter + 1;
      return res;
    }
    res.co2_conversion_per_pass = res.reactor.co2_conversion(inlet);

    res.reactor_outlet_plant_scale = res.reactor.outlet.toStream();
    for (auto& kv : res.reactor_outlet_plant_scale.molar_flow) {
      kv.second *= static_cast<double>(res.n_tubes_used);
    }

    // Effluent cooling to the drum temperature, with condensation
    const double drum_P_Pa = cfg.knockout_at_reactor_outlet_pressure
                                 ? res.reactor.outlet.P_Pa
                                 : units::barToPa(cfg.plant_cfg.knockout_P_bar);

    // Van-Dal's HX4. Inlet temperature is specified, so this cannot move the
    // fixed point. Uses the CURRENT effluent: no lag into the tear iteration
    Stream cooler_inlet = res.reactor_outlet_plant_scale;
    res.fehe = front_end::TwoStreamHXResult{};
    if (cfg.use_fehe && cfg.fehe_UA_W_per_K > 0.0) {
      const front_end::TwoStreamHXResult x = front_end::exchange_two_streams(
          res.reactor_outlet_plant_scale, mixed, cfg.fehe_UA_W_per_K, cfg.fehe_arrangement);
      // Never recover more than the feed actually needs: the trim heater has a
      // floor of zero, and any surplus stays in the effluent for the cooler
      if (x.ok && x.duty_MW > 0.0 && x.duty_MW <= res.feed_preheat_duty_MW) {
        res.fehe = x;
        cooler_inlet = x.hot_outlet;
      } else if (x.ok && x.duty_MW > res.feed_preheat_duty_MW) {
        const front_end::HXResult capped = front_end::heat_with_duty(
            res.reactor_outlet_plant_scale, -res.feed_preheat_duty_MW);
        if (capped.ok) {
          res.fehe = x;
          res.fehe.duty_MW = res.feed_preheat_duty_MW;
          res.fehe.cold_outlet = heated.outlet;
          res.fehe.hot_outlet = capped.outlet;
          cooler_inlet = capped.outlet;
        }
      }
    }
    res.trim_heater_duty_MW = std::max(0.0, res.feed_preheat_duty_MW - res.fehe.duty_MW);

    // Both duties computed directly, never one from the other by subtraction:
    // the exchanger uses a mean cp while the cooler integrates the real
    // enthalpy path including condensation, so a subtraction drifts
    const front_end::CondensingCoolResult full = front_end::cool_with_condensation(
        res.reactor_outlet_plant_scale, cfg.plant_cfg.knockout_T_K, drum_P_Pa);
    if (full.ok) {
      res.effluent_cooling_duty_MW = full.duty_MW;
      res.effluent_latent_fraction = full.latent_fraction;
    }

    const front_end::CondensingCoolResult cooled = front_end::cool_with_condensation(
        cooler_inlet, cfg.plant_cfg.knockout_T_K, drum_P_Pa);
    if (cooled.ok) res.trim_cooler_duty_MW = cooled.duty_MW;

    res.knockout =
        front_end::separate(res.reactor_outlet_plant_scale, cfg.plant_cfg.knockout_T_K, drum_P_Pa);
    if (!res.knockout.ok) {
      res.message = "knockout drum failed: " + res.knockout.message;
      res.n_iterations = iter + 1;
      return res;
    }

    res.recycle_split = front_end::split_recycle_purge(res.knockout.vapor, cfg.recycle_cfg);
    if (!res.recycle_split.ok) {
      res.message = "recycle split failed: " + res.recycle_split.message;
      res.n_iterations = iter + 1;
      return res;
    }

    // Purge membrane. The permeate can be routed back into the loop
    res.purge_membrane =
        front_end::separate_h2_membrane(res.recycle_split.purge, cfg.membrane_cfg);

    Stream g = res.recycle_split.recycle;   // g(x_n)
    if (cfg.route_permeate_to_feed && res.purge_membrane.ok) {
      g = front_end::mix_streams({g, res.purge_membrane.permeate});
    }
    // Recycle re-enters COLD: the circulator rejects its heat of compression
    // Handing it back at reactor temperature would hide most of the pre-heat
    g.temperature = cfg.plant_cfg.knockout_T_K;
    g.pressure    = units::barToPa(cfg.plant_cfg.reactor_inlet_P_bar);

    // ---- convergence test --------------------------------------------------
    res.final_residual = max_relative_change(tear, g);
    if (res.final_residual < cfg.tol_rel) {
      res.n_iterations = iter + 1;
      res.converged = true;
      break;
    }

    // ---- next iterate: Wegstein, or plain damped substitution --------------
    Stream next;
    next.temperature = g.temperature;
    next.pressure    = g.pressure;

    for (int i = 0; i < static_cast<int>(Species::Count); ++i) {
      const Species sp = static_cast<Species>(i);
      const double x_n = species_flow(tear, sp);
      const double g_n = species_flow(g, sp);

      double v;
      if (cfg.use_wegstein && have_previous) {
        const double x_p = species_flow(prev_tear, sp);
        const double g_p = species_flow(prev_g, sp);
        const double dx = x_n - x_p;
        if (std::abs(dx) > 1e-30) {
          double q = (g_n - g_p) / dx;
          q = std::min(std::max(q, cfg.wegstein_q_min), cfg.wegstein_q_max);
          const double denom = 1.0 - q;
          v = (std::abs(denom) > 1e-12) ? x_n + (g_n - x_n) / denom : g_n;
        } else {
          v = g_n;
        }
      } else {
        v = x_n + cfg.relaxation * (g_n - x_n);
      }

      if (!(v > 0.0) || !std::isfinite(v)) v = 0.0;   // an extrapolation must not go negative
      if (v > 0.0) next.molar_flow[sp] = v;
    }

    prev_tear = tear;
    prev_g    = g;
    have_previous = true;
    tear = next;
    res.n_iterations = iter + 1;
  }

  if (!res.converged) {
    res.message = "recycle loop did not converge within max_iterations";
    return res;
  }

  // ---- converged: circulator, booster, and the loop-boundary balances ------
  const double P_in_bar  = units::paToBar(res.knockout.vapor.pressure > 0.0
                                              ? res.knockout.vapor.pressure
                                              : res.reactor.outlet.P_Pa);
  const double P_out_bar = cfg.plant_cfg.reactor_inlet_P_bar;
  if (P_out_bar > P_in_bar) {
    res.circulator = front_end::run_compressor_stage_mixture(
        res.recycle_split.recycle, P_in_bar, P_out_bar, cfg.recycle_compressor_cfg);
  }

  if (cfg.route_permeate_to_feed && res.purge_membrane.ok) {
    // A negative setting means the module's own contract: both outlets leave at
    // the membrane's feed pressure, which here is the drum pressure. The
    // booster still has work to do, lifting that back to the reactor inlet
    res.permeate_pressure_assumed_at_feed = !(cfg.membrane_permeate_P_bar > 0.0);
    res.permeate_delivery_P_bar = res.permeate_pressure_assumed_at_feed
                                      ? P_in_bar
                                      : cfg.membrane_permeate_P_bar;
    if (P_out_bar > res.permeate_delivery_P_bar) {
      res.permeate_booster = front_end::run_compressor_stage_mixture(
          res.purge_membrane.permeate, res.permeate_delivery_P_bar, P_out_bar,
          cfg.recycle_compressor_cfg);
    }
  }

  // Methanol leaving the loop boundary: condensate plus whatever the purge
  // carries out
  const double meoh_liquid = species_flow(res.knockout.liquid, Species::CH3OH);
  double meoh_purge = species_flow(res.recycle_split.purge, Species::CH3OH);
  if (cfg.route_permeate_to_feed && res.purge_membrane.ok) {
    meoh_purge = species_flow(res.purge_membrane.retentate, Species::CH3OH);
  }
  res.meoh_production_kg_s = (meoh_liquid + meoh_purge) * M_meoh / 1000.0;

  // Overall conversion against the fresh CO2 fed, counting CO2 that leaves
  const double co2_in = species_flow(res.fresh_feed, Species::CO2);
  double co2_out = species_flow(res.knockout.liquid, Species::CO2);
  co2_out += (cfg.route_permeate_to_feed && res.purge_membrane.ok)
                 ? species_flow(res.purge_membrane.retentate, Species::CO2)
                 : species_flow(res.recycle_split.purge, Species::CO2);
  if (co2_in > 0.0) res.co2_conversion_overall = (co2_in - co2_out) / co2_in;

  // ---- loop diagnostics ---------------------------------------------------
  // SN is undefined without carbon oxides, so it is guarded rather than left
  // to throw on a degenerate stream
  const double cox = species_flow(res.converged_reactor_inlet, Species::CO)
                   + species_flow(res.converged_reactor_inlet, Species::CO2);
  if (cox > 0.0) {
    res.loop_stoichiometric_number =
        front_end::stoichiometric_number(res.converged_reactor_inlet);
  }

  res.peak_bed_temperature_K = res.reactor.outlet.T_K;
  for (const auto& pt : res.reactor.profile) {
    if (pt.T_K > res.peak_bed_temperature_K) res.peak_bed_temperature_K = pt.T_K;
  }

  // Lower heating values from the formation-enthalpy table, not a second set
  // of tabulated constants
  const thermo::Reaction burn_h2 = {
      {Species::H2, -1.0}, {Species::O2, -0.5}, {Species::H2O, 1.0}};
  const thermo::Reaction burn_co = {
      {Species::CO, -1.0}, {Species::O2, -0.5}, {Species::CO2, 1.0}};
  const thermo::Reaction burn_meoh = {
      {Species::CH3OH, -1.0}, {Species::O2, -1.5}, {Species::CO2, 1.0}, {Species::H2O, 2.0}};
  const thermo::Reaction burn_ch4 = {
      {Species::CH4, -1.0}, {Species::O2, -2.0}, {Species::CO2, 1.0}, {Species::H2O, 2.0}};

  const Stream& tail = (cfg.route_permeate_to_feed && res.purge_membrane.ok)
                           ? res.purge_membrane.retentate
                           : res.recycle_split.purge;
  res.tail_gas_LHV_MW =
      (species_flow(tail, Species::H2)    * -thermo::deltaH(burn_h2, 298.15)
     + species_flow(tail, Species::CO)    * -thermo::deltaH(burn_co, 298.15)
     + species_flow(tail, Species::CH3OH) * -thermo::deltaH(burn_meoh, 298.15)
     + species_flow(tail, Species::CH4)   * -thermo::deltaH(burn_ch4, 298.15)) / 1.0e6;

  res.product_LHV_MW = res.meoh_production_kg_s * 1000.0 / M_meoh
                       * -thermo::deltaH(burn_meoh, 298.15) / 1.0e6;

  res.ok = true;
  res.message = "ok";
  return res;
}

}  // namespace detail

namespace detail {

// Molar H2 flow for a given H2:CO2 ratio, expressed as a mass rate
double h2_kg_s_for_ratio(double m_dot_CO2_kg_s, double ratio) {
  const double n_CO2 = m_dot_CO2_kg_s * 1000.0 / properties(Species::CO2).molar_mass;
  return n_CO2 * ratio * properties(Species::H2).molar_mass / 1000.0;
}

// Applies the fresh hydrogen policy for the paths that do not run the outer
// controller. Shared with the aged entry point so the two cannot drift: before
// this existed, the aged path bypassed the policy entirely and rejected a
// caller who passed zero hydrogen expecting the ratio to supply it
double resolve_fresh_h2_kg_s(double m_dot_CO2_kg_s, double m_dot_H2_arg_kg_s,
                              const RecycleLoopConfig& cfg) {
  if (cfg.feed_ratio_mode == FeedRatioMode::FromArgument) return m_dot_H2_arg_kg_s;
  return h2_kg_s_for_ratio(m_dot_CO2_kg_s, cfg.fresh_h2_to_co2_ratio);
}

}  // namespace detail

namespace {
using detail::h2_kg_s_for_ratio;
}  // namespace

RecycleLoopResult run_co2_h2_plant_with_recycle(double m_dot_CO2_fresh_kg_s,
                                                 double m_dot_H2_fresh_kg_s,
                                                 const RecycleLoopConfig& cfg) {
  // FromArgument is the historical path and must remain bit-identical, so it
  // does not go near the outer loop
  if (cfg.feed_ratio_mode == FeedRatioMode::FromArgument) {
    RecycleLoopResult r = detail::run_recycle_with_activity(
        m_dot_CO2_fresh_kg_s, m_dot_H2_fresh_kg_s, cfg, 1.0);
    r.feed_ratio.mode = FeedRatioMode::FromArgument;
    if (m_dot_CO2_fresh_kg_s > 0.0) {
      const double nC = m_dot_CO2_fresh_kg_s * 1000.0 / properties(Species::CO2).molar_mass;
      const double nH = m_dot_H2_fresh_kg_s * 1000.0 / properties(Species::H2).molar_mass;
      r.feed_ratio.ratio_used = (nC > 0.0) ? nH / nC : 0.0;
    }
    r.feed_ratio.inlet_SN_achieved = r.loop_stoichiometric_number;
    r.feed_ratio.message = "hydrogen taken as supplied by the caller";
    return r;
  }

  if (cfg.feed_ratio_mode == FeedRatioMode::FixedRatio) {
    RecycleLoopResult r = detail::run_recycle_with_activity(
        m_dot_CO2_fresh_kg_s, h2_kg_s_for_ratio(m_dot_CO2_fresh_kg_s, cfg.fresh_h2_to_co2_ratio),
        cfg, 1.0);
    r.feed_ratio.mode = FeedRatioMode::FixedRatio;
    r.feed_ratio.ratio_used = cfg.fresh_h2_to_co2_ratio;
    r.feed_ratio.inlet_SN_achieved = r.loop_stoichiometric_number;
    r.feed_ratio.message = "hydrogen set from the configured fresh ratio";
    return r;
  }

  // Secant on Phi(R) = SN_inlet(R) - SN_target, bracketed by the bounds
  // The target is NOT always reachable: the inlet is dominated by CO2-depleted
  // recycle, so achievable inlet SN has a floor above 2. A search that hits a
  // bound reports target_reachable = false rather than returning the endpoint
  const double R_lo = cfg.feed_ratio_min, R_hi = cfg.feed_ratio_max;
  auto clamp = [&](double R){ return std::min(std::max(R, R_lo), R_hi); };

  RecycleLoopResult ra, rb, best;
  auto eval = [&](double R, RecycleLoopResult& out) -> double {
    out = detail::run_recycle_with_activity(
        m_dot_CO2_fresh_kg_s, h2_kg_s_for_ratio(m_dot_CO2_fresh_kg_s, R), cfg, 1.0);
    return out.ok ? (out.loop_stoichiometric_number - cfg.target_inlet_SN)
                  : std::numeric_limits<double>::quiet_NaN();
  };

  double Ra = clamp(cfg.fresh_h2_to_co2_ratio);
  double Rb = clamp(Ra - 0.25);
  double fa = eval(Ra, ra);
  double fb = eval(Rb, rb);

  // Track the best evaluated point explicitly. 
  // convergence against a secant variable that had already been shifted, and
  // reported a stale result as converged
  double R_best, f_best;
  if (rb.ok)      { best = rb; R_best = Rb; f_best = fb; }
  else            { best = ra; R_best = Ra; f_best = fa; }

  bool converged = std::isfinite(f_best) && std::abs(f_best) < cfg.feed_ratio_tol;
  bool hit_bound = false;
  int iter = 2;

  while (!converged && iter < cfg.feed_ratio_max_iter) {
    if (!std::isfinite(fa) || !std::isfinite(fb)) break;
    const double denom = fb - fa;
    if (std::abs(denom) < 1e-14) break;

    double Rc = Rb - fb * (Rb - Ra) / denom;
    if (!std::isfinite(Rc) || Rc <= R_lo || Rc >= R_hi) { Rc = clamp(Rc); hit_bound = true; }

    RecycleLoopResult rc;
    const double fc = eval(Rc, rc);
    ++iter;

    if (rc.ok) {
      best = rc; R_best = Rc; f_best = fc;
      if (std::abs(fc) < cfg.feed_ratio_tol) converged = true;
    }
    Ra = Rb; fa = fb; ra = rb;
    Rb = Rc; fb = fc; rb = rc;
    if (hit_bound) break;
  }

  const bool reachable = converged;

  best.feed_ratio.mode = FeedRatioMode::TargetInletSN;
  best.feed_ratio.ratio_used = R_best;
  best.feed_ratio.inlet_SN_achieved = best.loop_stoichiometric_number;
  best.feed_ratio.outer_iterations = iter;
  best.feed_ratio.converged = converged;
  best.feed_ratio.target_reachable = reachable;
  best.feed_ratio.message =
      converged ? "inlet SN target met"
                : (reachable ? "outer loop did not converge within feed_ratio_max_iter"
                             : "inlet SN target is not reachable on this bed and purge; "
                               "the achievable inlet SN has a floor set by the recycle");
  return best;
}

}  // namespace flowsheet
