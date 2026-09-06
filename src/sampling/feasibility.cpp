#include "sampling/feasibility.hpp"

#include <string>

namespace feasibility {

const char* to_string(Outcome o) {
  switch (o) {
    case Outcome::Feasible:                return "feasible";
    case Outcome::InvalidInput:            return "invalid_input";
    case Outcome::ReactorTemperatureBound: return "reactor_temperature_bound";
    case Outcome::ReactorPressureBound:    return "reactor_pressure_bound";
    case Outcome::ReactorRhsFailure:       return "reactor_rhs_failure";
    case Outcome::ReactorClampViolation:   return "reactor_clamp_violation";
    case Outcome::ReactorGeometryInvalid:  return "reactor_geometry_invalid";
    case Outcome::FlashFailure:            return "flash_failure";
    case Outcome::RecycleNotConverged:     return "recycle_not_converged";
    case Outcome::CompressionFailure:      return "compression_failure";
    case Outcome::EconomicsFailure:        return "economics_failure";
    case Outcome::UnknownFailure:          return "unknown_failure";
  }
  return "unknown_failure";
}

int to_code(Outcome o) { return static_cast<int>(o); }

namespace {

bool contains(const std::string& hay, const char* needle) {
  return hay.find(needle) != std::string::npos;
}

// Maps error strings to ML-friendly enums; unmatched strings safely default to UnknownFailure
Outcome classifyMessage(const std::string& m) {
  if (contains(m, "clamp absorbed a physically significant"))  return Outcome::ReactorClampViolation;
  if (contains(m, "temperature left the configured bounds"))    return Outcome::ReactorTemperatureBound;
  if (contains(m, "pressure fell below the configured minimum"))return Outcome::ReactorPressureBound;
  if (contains(m, "rhs failed"))                                return Outcome::ReactorRhsFailure;
  if (contains(m, "invalid bed geometry"))                      return Outcome::ReactorGeometryInvalid;
  if (contains(m, "invalid catalyst mass"))                     return Outcome::ReactorGeometryInvalid;

  if (contains(m, "flash threw") || contains(m, "flash did not converge"))
    return Outcome::FlashFailure;

  if (contains(m, "did not converge") || contains(m, "not converge"))
    return Outcome::RecycleNotConverged;

  if (contains(m, "must be positive") || contains(m, "must be in") ||
      contains(m, "must be non-negative") || contains(m, "rejected"))
    return Outcome::InvalidInput;

  if (contains(m, "compression") || contains(m, "compressor"))
    return Outcome::CompressionFailure;

  if (contains(m, "CEPCI") || contains(m, "economics") || contains(m, "OPEX") ||
      contains(m, "CAPEX"))
    return Outcome::EconomicsFailure;

  return Outcome::UnknownFailure;
}

}

Classification classify_reactor(const reactor::ReactorResult& res) {
  Classification c;
  c.raw_message = res.message;
  if (res.ok) {
    c.outcome  = Outcome::Feasible;
    c.feasible = true;
    return c;
  }
  c.outcome  = classifyMessage(res.message);
  c.feasible = false;
  return c;
}

Classification classify(const flowsheet::HybridPlantDesignPointResult& res) {
  Classification c;
  c.raw_message = res.message;

  if (res.ok) {
    c.outcome  = Outcome::Feasible;
    c.feasible = true;
    return c;
  }

  c.feasible = false;

  // Extract the innermost reactor failure first, as it provides a sharper gradient for the ML constraint classifier
  const auto& chain = res.reactor_chain;
  if (!chain.ok && !chain.message.empty()) {
    const Outcome inner = classifyMessage(chain.message);
    if (inner != Outcome::UnknownFailure) {
      c.outcome = inner;
      c.raw_message = chain.message;
      return c;
    }
  }

  if (!res.compression.ok && !res.compression.message.empty()) {
    c.outcome = Outcome::CompressionFailure;
    c.raw_message = res.compression.message;
    return c;
  }

  if (!res.capex.ok || !res.annual_opex.ok) {
    c.outcome = Outcome::EconomicsFailure;
    return c;
  }

  c.outcome = classifyMessage(res.message);
  return c;
}

}
