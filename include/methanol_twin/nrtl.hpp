#pragma once
#include <unordered_map>
#include "species.hpp"
#include "stream.hpp"

namespace nrtl {

  struct BinaryParams {
    double a_ij = 0.00, b_ij = 0.00;
    double a_ji = 0.00, b_ji = 0.00;
    double alpha = 0.30;
  };

  const BinaryParams& lookupParams(Species i, Species j);

  // False means the pair is treated as an ideal solution
  bool has_binary_params(Species i, Species j);

  // Count of pairs in the stream being treated as ideal
  int unparameterised_pairs(const Stream& liquidStream);

  double tau(Species i, Species j, double T);
  double G(Species i, Species j, double T);

  std::unordered_map<Species, double> activityCoefficients(const Stream& liquidStream, double T);
}
