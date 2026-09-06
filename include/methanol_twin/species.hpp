#pragma once

enum class Species { CO2, H2, CO, H2O, CH3OH, CH4, N2, Ar, O2, Count };

struct Properties {
  double molar_mass;
  double Tc;
  double Pc;
  double omega;
};

struct CpParams {
  int eq;
  double a, b, c, d, e, f, g;
};

const Properties& properties(Species sp);
const CpParams& cpParams(Species sp);

struct VaporPressureParams {
  double A, B, C, D, E;
};

const VaporPressureParams* vaporPressureParams(Species sp);
double vaporPressure(Species sp, double T);

const char* speciesName(Species sp);
