#pragma once

// Couples degradation/activity_decay.hpp to reactor/reactor_core.hpp without modifying either
// Calls the existing rate function, then multiplies by activity via activity_decay::scale_rate()

// Not a reimplementation of the reactor: every physical relationship is a call into reactor_core.hpp's public functions
// Only the RK4 control loop is new, in the same fixed-step form used elsewhere in the project

// Quasi-steady-state coupling, our choice and not from any source paper

// The reactor integrates over catalyst mass at steady state; decay integrates over real time
// Residence time here is seconds against aging runs of hundreds to thousands of hours, a separation of about 1e6
// so activity is treated as frozen and uniform during one pass 
// Hence a single scalar activity per call and a sequence of snapshots rather than a space-time solution

// Both reactions share one activity multiplier. 
// Fichtl measures activity from overall COx conversion and does not resolve methanol synthesis from RWGS separately
// This is defensible but not verified by the source

// Times are hours throughout, matching activity_decay.hpp

#include <string>
#include <vector>

#include "degradation/activity_decay.hpp"
#include "reactor/reactor_core.hpp"

namespace integration {

// One steady-state snapshot at a uniform activity, 1.0 being fresh
reactor::ReactorResult integrate_aged_reactor(const reactor::ReactorState& inlet,
                                               const reactor::ReactorConfig& cfg,
                                               double activity);

struct AgingCheckpoint {
  double t_h = 0.0;
  double activity = 1.0;
  reactor::ReactorResult reactor;
};

struct AgingCampaignResult {
  activity_decay::ActivityIntegrationResult decay;
  std::vector<AgingCheckpoint> checkpoints;
  bool ok = false;
  std::string message;
};

// Integrates a(t) once over [0, last checkpoint], then runs the reactor at
// each checkpoint's activity by lookup, not re-integration
// checkpoint_times_h must be non-empty, ascending, and within t_end_h
AgingCampaignResult run_aging_campaign(const reactor::ReactorState& inlet,
                                        const reactor::ReactorConfig& reactor_cfg,
                                        const activity_decay::ActivityDecayConfig& decay_cfg,
                                        double aging_T_K, double t_end_h, int decay_steps,
                                        const std::vector<double>& checkpoint_times_h);

}
