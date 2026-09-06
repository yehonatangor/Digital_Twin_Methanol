#pragma once
#include <unordered_map>
#include "species.hpp"
#include "stream.hpp"

namespace eos{

  struct PureParams{
    double a;
    double b;
  };

  struct MixtureParams {
    double a_mix = 0.00;
    double b_mix = 0.00;
    std::unordered_map<Species, PureParams> pure;
  };

  enum class RootSelect { 
    Vapor, 
    Liquid 
  };

  double kij(Species i, Species j);

  PureParams pureComponentParams(Species sp, double T);
  MixtureParams mixtureParams(const Stream& stream, double T);

  double compressibilityFactor(const MixtureParams& mix, double T, double P, RootSelect which);
  double molarVolume(double Z, double T, double P);

  std::unordered_map<Species, double> fugacityCoefficients(
    const MixtureParams& mix, const Stream& stream, double T, double P, double Z
  );
}
