#pragma once
#include <unordered_map>
#include "species.hpp"
#include "stream.hpp"

namespace flash {

  enum class Regime { 
    PhiPhi, 
    GammaPhi 
  };

  struct FlashResult {
    double vapor_fraction = 0.00;
    Stream vapor;
    Stream liquid;
    std::unordered_map<Species, double> K;
    Regime regime_used;
    bool converged = false;
    bool single_phase = false;

    // Gamma-phi only: liquid pairs treated as ideal. Always 0 on phi-phi
    int ideal_binary_pairs = 0;
  };

  Regime decideRegime(double P);
  FlashResult solve(const Stream& feed, double T, double P);
  FlashResult solve(const Stream& feed, double T, double P, Regime regime);

}
