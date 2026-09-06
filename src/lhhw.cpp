#include "lhhw.hpp"
#include <cmath>
#include "units.hpp"

namespace lhhw {

  namespace {

    // k_i = A_i * exp(B_i / (R*T)), B in J/mol. Van-Dal & Bouallou (2013)
    // J. Cleaner Production 57, 38-45, Table 3. Rate forms are Vanden Bussche
    // & Froment (1996); these parameters are the Mignard & Pritchard (2008)
    // refit Van-Dal Sec. 2.3.2 adopts, valid to 75 bar
    constexpr double A_K1 = 1.07;
    constexpr double A_K2 = 3453.38;
    constexpr double A_K3 = 0.499;
    constexpr double A_K4 = 6.62e-11;
    constexpr double A_K5 = 1.22e10;
    constexpr double B_K1 = 40000.00;
    constexpr double B_K3 = 17197.00;
    constexpr double B_K4 = 124119.00;
    constexpr double B_K5 = -98084.00;

    double k1_(double T) {
      return A_K1 * std::exp(B_K1 /(units::R * T));
    }
    double K2_() {
      return A_K2;
    }
    double K3_(double T) {
      return A_K3 * std::exp(B_K3 /(units::R * T));
    }
    double K4_(double T) {
      return A_K4 * std::exp(B_K4 /(units::R * T));
    }
    double k5_(double T) {
      return A_K5 * std::exp(B_K5 /(units::R * T));
    }
    double denominator(double P_H2, double P_H2O, double T) {
      return 1.00 + K2_() * (P_H2O / P_H2) + K3_(T) * std::sqrt(P_H2) + K4_(T) * P_H2O;
    }
  }

  // CO2 + 3H2 <-> CH3OH + H2O, bar^-2. Van-Dal Eq. (8), from Graaf et al. (1986), as printed
  // Partial pressures throughout are in bar
  double Keq1(double T) {
    return std::pow(10.0, 3066.00/ T - 10.592);
  }

  // Water-gas shift constant used in the RWGS driving force. Van-Dal Eq. (9) is printed with both signs inverted
  // The form here is Graaf's original, log10(K) = 2073/T - 2.029, which is the only one that makes Eq. (6) vanish at equilibrium
  // See docs/08
  double Keq2(double T) {
    return std::pow(10.0, 2073.0 / T - 2.029);
  }

  double r_CH3OH(double P_CO2, double P_H2, double P_H2O, double P_CH3OH, double T) {
    double D = denominator(P_H2, P_H2O, T);
    double bracket = 1.0 - (1.0 / Keq1(T)) * (P_H2O * P_CH3OH) / (P_H2 * P_H2 * P_H2 * P_CO2);
    return k1_(T) * P_CO2 * P_H2 * bracket / (D * D * D);
  }

  double r_RWGS(double P_CO2, double P_H2, double P_H2O, double P_CO, double T) {
    double D = denominator(P_H2, P_H2O, T);
    double bracket = 1.0 - Keq2(T) * (P_H2O * P_CO) / (P_CO2 * P_H2);
    return k5_(T) * P_CO2 * bracket / D;
  }
}
