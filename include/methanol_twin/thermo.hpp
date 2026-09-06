#pragma once
#include <initializer_list>
#include "species.hpp"

namespace thermo {

  struct Term {
    Species sp;
    double nu;
  };

  using Reaction = std::initializer_list<Term>;

  double cp(Species sp, double T);
  double enthalpy(Species sp, double T);
  double entropy(Species sp, double T);
  double deltaH(Reaction rxn, double T);
  double deltaS(Reaction rxn, double T);
  double deltaG(Reaction rxn, double T);
  double lnKeq(Reaction rxn, double T);
  double Keq(Reaction rxn, double T);

  void selfTest();

}
