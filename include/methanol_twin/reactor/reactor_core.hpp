#pragma once

// reactor_core.hpp: coupled single-stage packed-bed reactor

// Integrates simultaneously in catalyst-mass coordinate W [kg per tube]:
//   dF_i/dW = sum_j nu_ij * r_j (mole balance, 9 species)
//   dT/dW = reactor::dTdW_K_per_kg
//   dP/dW = reactor::pressure_gradient_dPdW

// Calls thermo::cp, lhhw::r_CH3OH / r_RWGS and eos::compressibilityFactor

// UNIT BOUNDARY: lhhw.hpp takes partial pressures in bar, the rest of the project in Pa 
// The conversion happens only in reaction_rates()

// Validation gate: Van-Dal Fig. A.1, molar fractions against reactor LENGTH, at Table A.2 catalyst and Table A.3 feed
// Pressure drop is negligible there (Re_p ~ 4), so that gate validates the kinetics and energy balance, not ergun.cpp


#include <string>
#include <vector>
#include "energy_balance.hpp"
#include "ergun.hpp"
#include "stream.hpp"
#include "transport.hpp"

namespace reactor {

// State at one integration point. All flows PER TUBE, indexed by Species
struct ReactorState {
  SpeciesArray F{}; // molar flows, mol/s
  double T_K  = 0.0;
  double P_Pa = 0.0;
  double W_kg = 0.0; // catalyst-mass coordinate, per tube

  double total_flow_mol_s() const;
  SpeciesArray mole_fractions() const;
  double mass_flow_kg_s() const;
  double mean_molar_mass_kg_mol() const;

  Stream toStream() const;
  static ReactorState fromStream(const Stream& s, double W_kg = 0.0);
};

struct ReactorConfig {
  BedGeometry bed{};
  PressureDropConfig dp{};
  ViscosityConfig visc{};
  ThermalMode thermal = ThermalMode::Adiabatic;
  CoolingConfig cooling{};

  // Integration grid. Accuracy is set by the step SIZE h = W_end / n_steps, not the count: 
  // the lab bed holds 0.0348 kg of catalyst per tube and the plant-scale bed 7.17 kg, 
  // so a count tuned on the lab bed is 206 times coarser on the plant bed
  // Measured cost of getting this wrong: n_steps = 500 reports 1.884 kg/s methanol against a grid-converged 1.711 kg/s
  
  // So the grid is now specified by a target step SIZE, with `n_steps` demoted to a lower BOUND on the count:
  
  // n_effective = max(n_steps, ceil(W_end / max_step_kg_cat))
  //
  // Any caller that previously pinned n_steps still gets at least that many steps
  // the change can only ever refine a grid, never coarsen one 
  // while a caller that leaves the defaults alone now gets a grid that scales with the bed it was handed

  int  n_steps = 500; // minimum step count
  double max_step_kg_cat = 2.0e-3; // target max step, kg cat/tube; <= 0 disables

  bool record_profile = true;
  bool use_real_gas_Z = true; // false => ideal gas Z = 1

  // Trip these and the integrator stops with a message
  double T_min_K  = 200.0;
  double T_max_K  = 900.0;
  double P_min_Pa = 1.0e4;

  // Clamp tolerance. RK4 can undershoot a near-depleted species by a rounding-level amount, and clamping that at zero is correct
  // But the same clamp, applied to a genuine overshoot from too large a step, silently CREATES moles (fabricating mass)
  // which can pass total-mass checks while poisoning the elemental balances
  
  // So the clamp now measures itself. If any step has to absorb a negative excursion larger 
  // than this fraction of the species' inlet flow
  // The integration STOPS and reports, instead of continuing on fabricated moles 
  // 1e-9 is far above float noise and far below anything physically real
  double max_clamp_rel = 1.0e-9;
};

struct ReactorResult {
  ReactorState outlet{};
  std::vector<ReactorState> profile;
  bool ok = false;
  std::string message;

  // Grid actually used, after max_step_kg_cat was applied
  int    n_steps_used = 0;
  double step_kg_cat  = 0.0;

  // Largest excursion the clamp absorbed. Healthy runs stay below 1e-12
  double max_clamp_rel = 0.0;

  double co2_conversion(const ReactorState& inlet) const;
  double meoh_yield_mol_s() const;
};

// Rates [mol/(kgcat.s)], {R_MEOH, R_RWGS}. Handles the Pa -> bar conversion
std::array<double, static_cast<std::size_t>(N_RXN)>
reaction_rates(const SpeciesArray& y, double T_K, double P_Pa);

double axial_position_m(double W_kg, const BedGeometry& bed);
double molar_density_mol_m3(const ReactorState& s, bool use_real_gas_Z);
double mass_density_kg_m3(const ReactorState& s, bool use_real_gas_Z);
double mass_flux_kg_m2s(const ReactorState& s, const BedGeometry& bed);

ReactorResult integrate_reactor(const ReactorState& inlet, const ReactorConfig& cfg);

// Van-Dal Table A.3: 2.8e-5 kg/s, 50 bar, 220 C, 4% CO / 82% H2 / 3% CO2 / 11% Ar
ReactorState van_dal_lab_feed();

void write_profile_csv(const ReactorResult& res, const BedGeometry& bed,
                       const std::string& path);

}

