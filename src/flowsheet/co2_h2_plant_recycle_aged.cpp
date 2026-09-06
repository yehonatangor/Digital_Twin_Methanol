#include "co2_h2_plant_recycle_aged.hpp"

namespace flowsheet {

namespace detail {
RecycleLoopResult run_recycle_with_activity(double m_dot_CO2_fresh_kg_s,
                                             double m_dot_H2_fresh_kg_s,
                                             const RecycleLoopConfig& cfg, double activity);
double resolve_fresh_h2_kg_s(double m_dot_CO2_kg_s, double m_dot_H2_arg_kg_s,
                              const RecycleLoopConfig& cfg);
}  // namespace detail

AgedRecycleLoopResult run_co2_h2_plant_with_recycle_aged(double m_dot_CO2_fresh_kg_s,
                                                          double m_dot_H2_fresh_kg_s,
                                                          const AgedRecycleLoopConfig& cfg) {
  AgedRecycleLoopResult res;
  res.activity = cfg.activity;

  // The whole loop is solved at this activity, so conversion, production and
  // the recycle composition all move with it. Activity 1.0 reproduces the
  // fresh loop exactly
  // The fresh hydrogen policy applies here too. TargetInletSN's outer secant is
  // NOT run on the aged path; that mode falls back to its configured ratio,
  // because searching the ratio and the activity together is a two-variable
  // problem this entry point does not claim to solve
  const double h2 = detail::resolve_fresh_h2_kg_s(m_dot_CO2_fresh_kg_s, m_dot_H2_fresh_kg_s,
                                                   cfg.recycle_cfg);
  res.recycle = detail::run_recycle_with_activity(m_dot_CO2_fresh_kg_s, h2,
                                                   cfg.recycle_cfg, cfg.activity);
  if (!res.recycle.ok) {
    res.message = "aged recycle loop failed: " + res.recycle.message;
    return res;
  }

  res.ok = true;
  res.message = "ok";
  return res;
}

}  // namespace flowsheet
