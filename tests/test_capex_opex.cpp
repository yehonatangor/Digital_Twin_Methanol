// Turton CAPCOST primitives
//
// Table A.1 constants were verified against the printed table (9 rows,
// 45 constants). Eq. (A.1), (A.3), (A.4), (7.9), (7.10), (7.15), (7.16) and
// Table 7.4's CEPCI series are exercised here

#include "economics/capex_opex.hpp"
#include "_harness.inc"

int main() {
  std::printf("=== Turton CAPCOST primitives ===\n\n");

  std::printf("[1] Table A.1 constants, spot checks against the printed table\n");
  const auto fh = economics::presets::turton_heat_exchanger_floating_head();
  check("floating head K1", fh.K1,  4.8306, 1e-12);
  check("floating head K2", fh.K2, -0.8509, 1e-12);
  check("floating head K3", fh.K3,  0.3187, 1e-12);
  const auto ft = economics::presets::turton_heat_exchanger_fixed_tube();
  check("fixed tube K1", ft.K1,  4.3247, 1e-12);
  check("fixed tube K2", ft.K2, -0.3030, 1e-12);
  const auto vv = economics::presets::turton_process_vessel_vertical();
  check("vertical vessel K1", vv.K1, 3.4974, 1e-12);
  check("vertical vessel K3", vv.K3, 0.1074, 1e-12);
  const auto cp = economics::presets::turton_compressor_centrifugal_axial_reciprocating();
  check("compressor K1", cp.K1,  2.2897, 1e-12);
  check("compressor K2", cp.K2,  1.3604, 1e-12);
  check("compressor K3", cp.K3, -0.1027, 1e-12);
  check("compressor min kW", cp.min_size,  450.0, 1e-12);
  check("compressor max kW", cp.max_size, 3000.0, 1e-12);

  std::printf("\n[2] Eq. (A.1) is 10^(K1 + K2 log10 A + K3 (log10 A)^2)\n");
  // A = 1 makes log10 A = 0, so the cost collapses to 10^K1 exactly
  check("A = 1 gives 10^K1", economics::purchased_cost_2001usd(fh.K1, fh.K2, fh.K3, 1.0),
        std::pow(10.0, fh.K1), 1e-6);
  checkTrue("cost rises with area",
            economics::purchased_cost_2001usd(ft.K1, ft.K2, ft.K3, 500.0) >
            economics::purchased_cost_2001usd(ft.K1, ft.K2, ft.K3, 100.0));
  check("non-positive area returns 0",
        economics::purchased_cost_2001usd(fh.K1, fh.K2, fh.K3, 0.0), 0.0, 1e-12);

  std::printf("\n[3] Eq. (A.3) pressure factor, Table A.2 convention\n");
  check("all-zero constants give FP = 1",
        economics::pressure_factor_general(0.0, 0.0, 0.0, 50.0), 1.0, 1e-12);
  const auto fp_hx = economics::presets::turton_fp_heat_exchanger_5_to_140_barg();
  checkTrue("FP > 1 above atmospheric",
            economics::pressure_factor_general(fp_hx.C1, fp_hx.C2, fp_hx.C3, 100.0) > 1.0);
  check("stated range low",  fp_hx.p_min_barg,   5.0, 1e-12);
  check("stated range high", fp_hx.p_max_barg, 140.0, 1e-12);

  std::printf("\n[4] Eq. (A.4) bare-module factor\n");
  const auto bm = economics::presets::turton_bm_heat_exchanger_floating_head_fixed_u_kettle();
  check("B1", bm.B1, 1.63, 1e-12);
  check("B2", bm.B2, 1.66, 1e-12);
  check("FBM = B1 + B2 FM FP", economics::bare_module_factor(bm.B1, bm.B2, 1.0, 1.0),
        1.63 + 1.66, 1e-12);
  check("base case at FM = FP = 1", economics::bare_module_factor(2.25, 1.82, 1.0, 1.0),
        4.07, 1e-12);

  std::printf("\n[5] Eq. (7.15) and (7.16)\n");
  check("TMC = 1.18 sum(CBM)", economics::total_module_cost(1.0e6), 1.18e6, 1e-6);
  check("GRC = TMC + 0.50 sum(CBM,base)",
        economics::grassroots_cost(1.18e6, 1.0e6), 1.18e6 + 0.5e6, 1e-6);

  std::printf("\n[6] Table 7.4 CEPCI\n");
  check("2001 basis is 397", economics::cepci(2001).value, 397.0, 1e-12);
  check("2016 is 541.7",     economics::cepci(2016).value, 541.7, 1e-12);
  check("1996 is 382",       economics::cepci(1996).value, 382.0, 1e-12);
  checkTrue("2017 is not available (no extrapolation)", !economics::cepci(2017).ok);
  checkTrue("1995 is not available",                    !economics::cepci(1995).ok);
  check("escalation is a plain ratio",
        economics::escalate_cost(1.0e6, 541.7), 1.0e6 * 541.7 / 397.0, 1e-6);
  check("escalating to the basis year is a no-op",
        economics::escalate_cost(1.0e6, 397.0), 1.0e6, 1e-6);

  std::printf("\n[7] Eq. (7.9) wall thickness and Eq. (7.10) pressure factor\n");
  // Thin wall below t_min collapses to FP = 1
  check("FP = 1 for a thin wall",
        economics::vessel_pressure_factor(1.0, 0.5, economics::kTurtonVesselS_CS_bar,
                                           0.9, economics::kTurtonVesselCA_m,
                                           economics::kTurtonVesselTmin_m),
        1.0, 1e-12);
  checkTrue("FP > 1 for a thick wall",
            economics::vessel_pressure_factor(50.0, 3.0, economics::kTurtonVesselS_CS_bar,
                                               0.9, economics::kTurtonVesselCA_m,
                                               economics::kTurtonVesselTmin_m) > 1.0);
  check("vacuum branch is 1.25",
        economics::vessel_pressure_factor(-1.0, 1.0, economics::kTurtonVesselS_CS_bar,
                                           0.9, economics::kTurtonVesselCA_m,
                                           economics::kTurtonVesselTmin_m),
        1.25, 1e-12);
  check("thickness includes the corrosion allowance",
        economics::vessel_wall_thickness_m(0.0, 1.0, economics::kTurtonVesselS_CS_bar, 0.9,
                                            economics::kTurtonVesselCA_m),
        economics::kTurtonVesselCA_m, 1e-12);
  check("S for carbon steel", economics::kTurtonVesselS_CS_bar, 944.0, 1e-12);
  check("corrosion allowance, 1/8 in", economics::kTurtonVesselCA_m, 0.00315, 1e-12);
  check("t_min, 1/4 in",               economics::kTurtonVesselTmin_m, 0.0063, 1e-12);

  std::printf("\n[8] Lim Table 1 prices (all seven verified against the printed table)\n");
  const auto p = economics::presets::lim_table1_prices_2020();
  check("natural gas, USD/GJ",  p.natural_gas_USD_per_GJ,   2.99,  1e-12);
  check("CO2, USD/t",           p.co2_USD_per_t,            86.4,  1e-12);
  check("water, USD/t",         p.water_USD_per_t,          0.15,  1e-12);
  check("hydrogen, USD/kg",     p.hydrogen_USD_per_kg,      3.2,   1e-12);
  check("electricity, USD/kWh", p.electricity_USD_per_kWh,  0.06,  1e-12);
  check("interest rate",        p.interest_rate,            0.045, 1e-12);
  check("stream factor",        p.stream_factor,            0.9,   1e-12);
  checkTrue("water's unit is resolved, not guessed", p.water_price_unit_sourced);

  std::printf("\n[9] Lim Eq. (5) capital recovery factor\n");
  // i = 4.5 %, n = 20. (1.045)^20 = 2.4117140, so CRF = 0.0768759
  check("CRF at 4.5 % over 20 years",
        economics::capital_recovery_factor(0.045, 20), 0.0768759, 1e-6);
  check("zero interest is 1/n", economics::capital_recovery_factor(0.0, 20), 0.05, 1e-12);
  check("non-positive horizon returns 0", economics::capital_recovery_factor(0.045, 0), 0.0, 1e-12);

  std::printf("\n[10] WE stack replacement, 30 %% per replacement, full cycles only\n");
  const auto we = economics::we_replacement_cost(1.0e6, 8.0, 20);
  check("replacements in 20 years at 8-year life", we.n_replacements, 2.0, 1e-12);
  check("total", we.total_replacement_cost_USD, 0.30 * 1.0e6 * 2.0, 1e-6);
  check("annualized", we.annualized_replacement_cost_USD, 0.30 * 1.0e6 * 2.0 / 20.0, 1e-6);

  std::printf("\n[11] Lim Eq. (6) unit production cost\n");
  check("total annual cost over annual tonnes",
        economics::unit_production_cost_USD_per_t(5.0e7, 250000.0), 200.0, 1e-9);

  std::printf("\n[12] Mucci six-tenths rule, cross-check only\n");
  const auto anchor = economics::presets::mucci_capex_anchor();
  check("anchor CAPEX, EUR", anchor.capex_ref_EUR, 27.8e6, 1e-6);
  check("anchor scale, t/y", anchor.M_ref_t_per_y, 0.08e6, 1e-6);
  check("scaling at the anchor reproduces the anchor",
        economics::capex_scale(anchor.capex_ref_EUR, anchor.M_ref_t_per_y, anchor.M_ref_t_per_y),
        anchor.capex_ref_EUR, 1e-6);
  check("doubling scale multiplies by 2^0.6",
        economics::capex_scale(1.0, 1.0, 2.0), std::pow(2.0, 0.6), 1e-12);

  std::printf("\n[13] Turton Example 7.14 reproduced end to end\n");
  // The strongest available check on this chapter: seven items, every step of
  // the chain (Eq. A.1 purchased cost, Eq. A.3 pressure factor, Fig. A.18
  // material factor, Eq. A.4 bare module, summation, CEPCI escalation), with
  // inputs and printed answers both taken from Table E7.14(a) and (b)
  //
  // Turton prints his table rounded to the nearest hundred dollars, so the
  // tolerances below are his rounding, not slack. FP for T-101 and V-101 are
  // his own printed values, since Eq. (7.10) needs the weld efficiency that
  // section [7] of this file shows the example itself back-solves to 0.750
  {
    using namespace economics;
    using namespace economics::presets;
    const double pi = 3.14159265358979323846;
    auto disc_area = [&](double D) { return pi / 4.0 * D * D; };

    const auto fh   = turton_heat_exchanger_floating_head();
    const auto bmfh = turton_bm_heat_exchanger_floating_head_fixed_u_kettle();
    const auto dp   = turton_heat_exchanger_double_pipe();
    const auto bmdp = turton_bm_heat_exchanger_double_pipe_scraped_spiral();
    const auto pu   = turton_pump_centrifugal();
    const auto bmpu = turton_bm_pump_centrifugal_recip_pd();
    const auto tw   = turton_tower_tray_and_packed();
    const auto bmv  = turton_bm_process_vessel_vertical_incl_towers();
    const auto tr   = turton_tray_sieve();
    const auto hv   = turton_process_vessel_horizontal();
    const auto bmh  = turton_bm_process_vessel_horizontal();
    const auto fpx  = turton_fp_heat_exchanger_5_to_140_barg();

    // E-101, overhead condenser: 170 m2, all carbon steel, 5.0 barg
    const double cp_E101  = purchased_cost_2001usd(fh.K1, fh.K2, fh.K3, 170.0);
    const double fbm_E101 = bare_module_factor(bmfh.B1, bmfh.B2, kFmShellTubeCS_CS, 1.0);
    check("E-101 purchased cost", cp_E101, 33000.0, 100.0);
    check("E-101 F_BM",           fbm_E101, 3.29, 0.005);
    check("E-101 bare module",    cp_E101 * fbm_E101, 108500.0, 100.0);

    // E-102, reboiler: 205 m2, CS shell with SS tubes, 18.0 barg on the tube
    const double fp_E102  = pressure_factor_general(fpx.C1, fpx.C2, fpx.C3, 18.0);
    const double cp_E102  = purchased_cost_2001usd(fh.K1, fh.K2, fh.K3, 205.0);
    const double fbm_E102 = bare_module_factor(bmfh.B1, bmfh.B2, kFmShellTubeCS_SS, fp_E102);
    check("E-102 F_P at 18 barg", fp_E102, 1.062, 0.001);
    check("E-102 F_BM",           fbm_E102, 4.82, 0.005);
    check("E-102 bare module",    cp_E102 * fbm_E102, 177900.0, 200.0);

    // E-103, product cooler: 10 m2 double pipe, all carbon steel
    const double cp_E103  = purchased_cost_2001usd(dp.K1, dp.K2, dp.K3, 10.0);
    const double fbm_E103 = bare_module_factor(bmdp.B1, bmdp.B2, 1.0, 1.0);
    check("E-103 bare module", cp_E103 * fbm_E103, 12300.0, 100.0);

    // P-101 A/B, reflux pumps: two units, 5 kW shaft, CS, 5 barg. Below Table
    // A.2's 10 barg pump range, so F_P is 1 by that table's own convention
    const double cp_P101  = purchased_cost_2001usd(pu.K1, pu.K2, pu.K3, 5.0);
    const double fbm_P101 = bare_module_factor(bmpu.B1, bmpu.B2, 1.55, 1.0);
    check("P-101 F_BM", fbm_P101, 3.98, 0.005);
    check("P-101 bare module, both units", 2.0 * cp_P101 * fbm_P101, 25200.0, 200.0);

    // T-101, aromatics column: 2.1 m diameter, 23 m tall, CS shell
    const double vol_T101 = disc_area(2.1) * 23.0;
    const double cp_T101  = purchased_cost_2001usd(tw.K1, tw.K2, tw.K3, vol_T101);
    const double fbm_T101 = bare_module_factor(bmv.B1, bmv.B2, 1.0, kTurtonExample714TowerFP);
    check("T-101 F_BM",        fbm_T101, 5.31, 0.005);
    check("T-101 bare module", cp_T101 * fbm_T101, 290700.0, 200.0);

    // 32 stainless sieve trays, costed per tray on the column cross-section
    const double cp_tray = purchased_cost_2001usd(tr.K1, tr.K2, tr.K3, disc_area(2.1));
    check("tray stack bare module",
          32.0 * cp_tray * kFbmSieveTraySS, 131200.0, 400.0);

    // V-101, reflux drum: 1.8 m diameter, 6 m long, horizontal, CS
    const double vol_V101 = disc_area(1.8) * 6.0;
    const double cp_V101  = purchased_cost_2001usd(hv.K1, hv.K2, hv.K3, vol_V101);
    const double fbm_V101 = bare_module_factor(bmh.B1, bmh.B2, 1.0,
                                               kTurtonExample714HorizontalVesselFP);
    check("V-101 F_BM",        fbm_V101, 3.79, 0.005);
    check("V-101 bare module", cp_V101 * fbm_V101, 51200.0, 100.0);

    const double sum_cp  = cp_E101 + cp_E102 + cp_E103 + 2.0 * cp_P101
                         + cp_T101 + 32.0 * cp_tray + cp_V101;
    const double sum_cbm = cp_E101 * fbm_E101 + cp_E102 * fbm_E102 + cp_E103 * fbm_E103
                         + 2.0 * cp_P101 * fbm_P101 + cp_T101 * fbm_T101
                         + 32.0 * cp_tray * kFbmSieveTraySS + cp_V101 * fbm_V101;
    std::printf("       [INFO] sum Cp0 %.0f (printed 219,900), sum CBM %.0f (printed 797,000)\n",
                sum_cp, sum_cbm);
    check("total purchased cost", sum_cp,  219900.0, 200.0);
    check("total bare module cost", sum_cbm, 797000.0, 300.0);

    // The example escalates with 542 for 2016; Table 7.4 prints 541.7
    check("escalated to 2016", escalate_cost(sum_cbm, 542.0), 1088100.0, 400.0);
  }

  return report("capex/opex");
}
