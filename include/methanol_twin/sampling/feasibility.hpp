#pragma once
// Why a design point failed

// A dropped row biases a training set, because failures cluster at the edges of the design space rather than scattering
// A NaN loses which wall was hit

// So a failure is a classification

// Integer codes are FROZEN: append to the end of the enum, never renumber, or previously generated datasets change meaning.
// The originating message is preserved verbatim, since classification is lossy

// Reads results that already exist and changes nothing
// See docs/25-model-provenance.md

#include <string>

#include "hybrid_plant_design_point.hpp"

namespace feasibility {

enum class Outcome {
  // Converged, all balances closed, every guard satisfied
  Feasible = 0,

  // Input-side rejections
  InvalidInput, // out-of-domain config: activity outside (0,1], negative feed, ..

  // Reactor-side failures
  ReactorTemperatureBound, // integration left [T_min_K, T_max_K] -- thermal runaway or quench
  ReactorPressureBound, // pressure fell below P_min_Pa
  ReactorRhsFailure, // RHS evaluation failed (non-finite state mid-step)
  ReactorClampViolation, // non-negativity clamp exceeded max_clamp_rel; atoms would not conserve
  ReactorGeometryInvalid, // bed produced non-positive catalyst mass

  // Separation / loop failures
  FlashFailure, // a flash threw or failed to converge
  RecycleNotConverged, // recycle loop hit its iteration limit

  // Downstream
  CompressionFailure,
  EconomicsFailure, // CEPCI unavailable, non-positive production, ..

  // Fallback
  UnknownFailure // ok = false with a message this classifier does not recognise
};

// Stable short token, suitable as a categorical value in a dataset
// Guaranteed not to contain commas, quotes or whitespace
const char* to_string(Outcome o);

// Stable integer code. 
// Values are FROZEN append new outcomes at the end of the enum, never renumber, or previously generated datasets change meaning
int to_code(Outcome o);

struct Classification {
  Outcome outcome = Outcome::UnknownFailure;
  bool feasible = false;
  // The originating message. 
  // Classification is a lossy projection of it; keep the original 
  // so an unrecognised failure mode can be diagnosed after the fact rather than requiring the sweep be re-run
  std::string raw_message;
};

// Classifies a design-point result. Recognises the specific message strings the
// integrators and flowsheets emit; anything unmatched becomes UnknownFailure
// WITH its message intact, which is the signal to add a case here
Classification classify(const flowsheet::HybridPlantDesignPointResult& res);

// Lower-level overload for sweeps that call the reactor directly
Classification classify_reactor(const reactor::ReactorResult& res);

}
