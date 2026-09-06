#pragma once
// The recycle loop solved at a reduced, frozen catalyst activity
//
// Wiring only. Runs co2_h2_plant_recycle.hpp with the reactor calls routed
// through integration/aged_reactor.hpp, which scales the LHHW rate by the
// activity. Adds no physics of its own and shares the fresh-hydrogen policy
// via detail::resolve_fresh_h2_kg_s
//
// activity is raw, in (0, 1], not a time on stream. Convert with
// degradation/activity_decay.hpp first if a campaign time is what is known
//
// Lower activity means less conversion per pass, more recycle for the same
// duty, and a larger Ergun drop, so the loop has an activity FLOOR that
// differs by operating branch. See docs/19-catalyst-deactivation.md

#include <string>
#include "flowsheet/co2_h2_plant_recycle.hpp"
#include "integration/aged_reactor.hpp"

namespace flowsheet {

struct AgedRecycleLoopConfig {
  RecycleLoopConfig recycle_cfg;    // SAME struct as Ch. 8.5 -- plant_cfg (incl. reactor_cfg.bed), recycle_cfg, convergence knobs
  double activity = 1.0;            // uniform, frozen catalyst activity actually used in the reactor call (1.0 = fresh). See header: raw (0,1], not time-on-stream
};

struct AgedRecycleLoopResult {
  RecycleLoopResult recycle;   // IDENTICAL shape to Ch. 8.5's own result -- same fields, same meaning, just computed at `activity` instead of always 1.0
  double activity = 1.0;       // echoed back for traceability (e.g. when logging sweep results)

  bool ok = false;
  std::string message;
};

// At activity=1.0, reproduces run_co2_h2_plant_with_recycle()'s own result
// bit-for-bit (checked directly, not assumed -- see
// test_co2_h2_plant_recycle_aged.cpp Section 1), since
// integration::integrate_aged_reactor() at activity=1.0 already reproduces
// reactor::integrate_reactor() to 1e-12 relative (Chapter 7.3)
AgedRecycleLoopResult run_co2_h2_plant_with_recycle_aged(
    double m_dot_CO2_fresh_kg_s, double m_dot_H2_fresh_kg_s,
    const AgedRecycleLoopConfig& cfg = AgedRecycleLoopConfig{});

}  // namespace flowsheet
