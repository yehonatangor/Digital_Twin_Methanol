#pragma once

namespace lhhw {

  double Keq1(double T);
  double Keq2(double T);

  double r_CH3OH(double P_CO2, double P_H2, double P_H2O, double P_CH3OH, double T);
  double r_RWGS(double P_CO2, double P_H2, double P_H2O, double P_CO, double T);

}
