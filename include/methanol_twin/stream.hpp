#pragma once
#include <unordered_map>
#include <stdexcept>
#include "species.hpp"

enum class Phase { Vapor, Liquid, Mixed };

class Stream{

    public:
      double pressure = 0.00;
      double temperature = 0.00;
      Phase phase = Phase::Vapor;
      std::unordered_map<Species, double> molar_flow;

    double totalMolarFlow() const{
      double sum = 0.0;
      for (const auto[key, value]: molar_flow){
          sum += value;
      }
        return sum;
    }

    double moleFraction(Species sp) const {

      double total = totalMolarFlow();
      if (total <= 0.00) throw std::runtime_error("Total Molar Flow is zero or negative");
      auto it = molar_flow.find(sp);
      return (it == molar_flow.end()) ?  0.00 : it -> second / total;

    }

    // Returns kg/s
    // Properties::molar_mass is g/mol, hence the /1000
    double massFlow(Species sp) const {
      auto it = molar_flow.find(sp);
      if (it == molar_flow.end()) return 0.00;
      return it->second * properties(sp).molar_mass / 1000.0; // g/mol -> kg/mol
    }

    double totalMassFlow() const{
        double sum = 0.0;
        for (const auto& [key,value] : molar_flow) {
          sum += value * properties(key).molar_mass / 1000.0; // g/mol -> kg/mol
        }
        return sum;
    }

    double massFraction(Species sp) const {
      double total = totalMassFlow();
      if (total <= 0.0) {
        throw std::runtime_error("Total mass flow is zero or negative");
      }
       return massFlow(sp) / total;
    }
};
