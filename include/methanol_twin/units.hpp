#pragma once

namespace units {

  inline constexpr double R = 8.314462618;
  inline constexpr double PA_PER_BAR = 100'000.0;
  inline constexpr double PA_PER_ATM = 101'325.0;

  [[nodiscard]]
  inline constexpr double barToPa(double pressureBar) noexcept {
    return pressureBar * PA_PER_BAR;
  }

  [[nodiscard]]
  inline constexpr double paToBar (double pressurePa) noexcept {
    return pressurePa / PA_PER_BAR;
  }

  [[nodiscard]]
  inline constexpr double atmToPa(double pressureAtm) noexcept {
    return pressureAtm * PA_PER_ATM;
  }

  [[nodiscard]]
  inline constexpr double paToAtm(double pressurePa) noexcept {
    return pressurePa / PA_PER_ATM;
  }

  inline constexpr double CELSIUS_TO_KELVIN_OFFSET = 273.15;
  [[nodiscard]]
  inline constexpr double celsiusToKelvin (double temperatureCelsius) noexcept {
    return temperatureCelsius + CELSIUS_TO_KELVIN_OFFSET;
  }

  [[nodiscard]]
  inline constexpr double kelvinToCelsius (double temperatureKelvin) noexcept {
    return temperatureKelvin - CELSIUS_TO_KELVIN_OFFSET;
  }

 inline constexpr double KG_PER_TON = 1'000.0;
 inline constexpr double SECONDS_PER_HOUR = 3'600.0;
 inline constexpr double KG_PER_SECOND_PER_TON_PER_HOUR = KG_PER_TON / SECONDS_PER_HOUR;

  [[nodiscard]]
  inline constexpr double tonsPerHourToKgPerSecond(double tonsPerHour) noexcept {
    return tonsPerHour * KG_PER_SECOND_PER_TON_PER_HOUR;
  }

  [[nodiscard]]
    inline constexpr double kgPerSecondToTonsPerHour( double kilogramsPerSecond) noexcept {
      return kilogramsPerSecond / KG_PER_SECOND_PER_TON_PER_HOUR;
    }
}

  static_assert(units::R == 8.314462618);
  static_assert(units::barToPa(1.0) == 100'000.0);
  static_assert(units::paToBar(units::barToPa(5.0)) == 5.0);
  static_assert(units::atmToPa(1.0) == 101'325);
  static_assert(units::paToAtm(units::atmToPa(2.0))==2.0);
  static_assert(units::celsiusToKelvin(0.0)==273.15);
  static_assert(units::kelvinToCelsius(units::celsiusToKelvin(25.0)) ==25.0);
  static_assert(units::kgPerSecondToTonsPerHour(units::tonsPerHourToKgPerSecond(18.0))==18.0 );
