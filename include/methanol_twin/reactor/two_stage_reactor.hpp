#pragma once

// two_stage_reactor.hpp -- Mucci's cooled two-stage configuration

// SOURCED (Mucci et al. 2023, Appendix A.4):
//   - two-stage multi-tubular reactor
//   - cooled via evaporating water at around 245 degC
//   - operating pressure around 75 bar
//   - 1 bar pressure drop assumed for each reactor stage
//   - intermediate removal of products via condensation to shift equilibrium
//   - LHHW kinetics, commercial Cu/ZnO/Al2O3 catalyst

// NO ISOLATED VALIDATION CASE EXISTS for this module in any of the four papers

// INTEGRATION NOTE, no FlashFn hook. 
// The interstage condenser calls
// flash::solve() directly through ReactorState::toStream(), so the twin has one
// flash implementation instead of an adapter layer wrapped around it

#include "flash.hpp"
#include "reactor_core.hpp"
#include "units.hpp"

namespace reactor {

struct TwoStageConfig {
  ReactorConfig stage1{};
  ReactorConfig stage2{};

  // Not sourced. Mucci gives no interstage temperature; 40 C is borrowed from Shi et al. (2020) Sec. 3.1
  // which is a final separator, not an interstage
  double interstage_T_K = units::celsiusToKelvin(40.0);
  bool   interstage_T_sourced = false;

  // Negative means stage-1 outlet pressure carries through unchanged
  double interstage_P_Pa = -1.0;

  double expected_outlet_P_Pa = units::barToPa(73.0);
};

struct TwoStageResult {
  ReactorResult stage1{};
  flash::FlashResult interstage{};
  ReactorResult stage2{};

  ReactorState outlet{};
  SpeciesArray crude_liquid{}; // interstage condensate only
  bool ok = false;
  std::string message;
};

TwoStageResult integrate_two_stage(const ReactorState& inlet,
                                   const TwoStageConfig& cfg);

// Mucci's operating data with the ConstantTotal (1 bar/stage) drop model
// Mucci publishes no tube dimensions, so the beds are caller-supplied
TwoStageConfig mucci_two_stage_config(const BedGeometry& bed_stage1,
                                      const BedGeometry& bed_stage2);

}

