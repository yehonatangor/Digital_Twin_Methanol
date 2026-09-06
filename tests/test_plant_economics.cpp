// CAPEX aggregation and OPEX, including the all-in-installed bypass

#include "economics/plant_economics.hpp"
#include "_harness.inc"

int main() {
  std::printf("=== plant economics ===\n\n");

  std::printf("[1] Natural gas LHV from the formation-enthalpy table\n");
  // CH4 + 2 O2 -> CO2 + 2 H2O(g). Handbook LHV is about 50.0 MJ/kg
  const double lhv = economics::natural_gas_lhv_MJ_per_kg();
  check("LHV, MJ/kg", lhv, 50.0, 0.6);

  std::printf("\n[2] The electrolyzer must NOT take Turton's markup twice\n");
  // A $1m all-in item plus a $1m Turton item. The all-in half must arrive at
  // face value; only the Turton half gets 1.18x and CEPCI
  economics::CapexLineItem turton;
  turton.name = "turton item";
  turton.Cp0_2001usd = 1.0e6; turton.FBM = 1.0;
  turton.CBM_2001usd = 1.0e6; turton.CBM_base_case_2001usd = 1.0e6;

  const auto we = economics::cost_electrolyzer("electrolyzer", 1000.0, 1000.0);
  checkTrue("electrolyzer is flagged all-in installed", we.all_in_installed);
  check("electrolyzer CBM is power x $/kW", we.CBM_2001usd, 1.0e6, 1e-6);

  const auto agg = economics::aggregate_capex({turton, we}, 2016);
  checkTrue("aggregation ok", agg.ok);
  check("all-in accumulator holds only the electrolyzer",
        agg.sum_all_in_installed_usd, 1.0e6, 1e-6);
  check("Turton accumulator holds only the Turton item", agg.sum_CBM_2001usd, 1.0e6, 1e-6);
  const double expect = 1.18e6 * 541.7 / 397.0 + 1.0e6;
  check("total = escalated Turton part + all-in at face value",
        agg.total_module_cost_escalated, expect, 1.0);
  checkTrue("the all-in part did NOT get the 1.61x inflation",
            agg.total_module_cost_escalated < 1.18e6 * 541.7 / 397.0 * 2.0);

  std::printf("\n[3] Reactor costed as a shell-and-tube exchanger\n");
  reactor::BedGeometry bed;
  bed.tube_inner_diameter_m = 0.035; bed.bed_length_m = 7.0; bed.n_tubes = 100;
  const auto rx = economics::cost_reactor_as_shell_and_tube("reactor", bed, 1.0, 1.0);
  const double area = 100.0 * 3.14159265358979323846 * 0.035 * 7.0;
  checkTrue("a positive cost is produced", rx.CBM_2001usd > 0.0);
  checkTrue("area is in Table A.1's range for this bed", area > 10.0 && area < 1000.0);
  checkTrue("cost rises with tube count", [&]{
    reactor::BedGeometry big = bed; big.n_tubes = 400;
    return economics::cost_reactor_as_shell_and_tube("r", big, 1.0, 1.0).CBM_2001usd >
           rx.CBM_2001usd; }());

  std::printf("\n[4] Compressor picks the right Fig. A.19 factor\n");
  const auto cs = economics::cost_compressor("c", 1000.0, economics::MaterialOfConstruction::CarbonSteel);
  const auto ss = economics::cost_compressor("c", 1000.0, economics::MaterialOfConstruction::StainlessSteel);
  check("carbon steel FBM",    cs.FBM, 2.75, 1e-12);
  check("stainless steel FBM", ss.FBM, 5.7,  1e-12);
  checkTrue("stainless costs more", ss.CBM_2001usd > cs.CBM_2001usd);
  checkTrue("out-of-range power is flagged in the name",
            economics::cost_compressor("c", 20000.0, economics::MaterialOfConstruction::CarbonSteel)
                .name.find("outside") != std::string::npos);

  std::printf("\n[5] OPEX\n");
  economics::ReactantFlowsKgPerS flows;
  flows.co2_kg_s = 24.4444;
  const auto prices = economics::presets::lim_table1_prices_2020();
  const auto we_repl = economics::we_replacement_cost(1.0e6, 8.0, 20);
  const auto opex = economics::compute_opex(flows, 10.0, prices, 0.9, 1.0e7, we_repl);
  checkTrue("opex ok", opex.ok);
  const double op_s = 365.0 * 24.0 * 3600.0 * 0.9;
  check("CO2 cost = t/y x $86.4", opex.reactant_cost_USD_per_y,
        24.4444 * op_s / 1000.0 * 86.4, 1.0);
  check("maintenance is 20 % of the stated CAPEX base",
        opex.maintenance_cost_USD_per_y, 0.20 * 1.0e7, 1e-6);
  check("WE replacement is carried through",
        opex.we_replacement_cost_USD_per_y, we_repl.annualized_replacement_cost_USD, 1e-6);
  check("total is the sum of the four lines", opex.total_opex_USD_per_y,
        opex.reactant_cost_USD_per_y + opex.electricity_cost_USD_per_y +
        opex.maintenance_cost_USD_per_y + opex.we_replacement_cost_USD_per_y, 1e-6);
  checkTrue("a stream factor above 1 is rejected",
            !economics::compute_opex(flows, 10.0, prices, 1.5, 0.0, we_repl).ok);

  std::printf("\n[6] Unit cost, Lim Eq. (6)\n");
  const auto pe = economics::run_plant_economics(1.0e8, opex, 0.045, 20, 250000.0);
  check("CRF", pe.crf, 0.0768761, 1e-6);
  check("annualized CAPEX", pe.annualized_capex_USD_per_y, 1.0e8 * pe.crf, 1e-3);
  check("total annual cost", pe.total_annual_cost_USD_per_y,
        pe.annualized_capex_USD_per_y + opex.total_opex_USD_per_y, 1e-3);
  check("unit cost", pe.unit_cost_USD_per_t,
        pe.total_annual_cost_USD_per_y / 250000.0, 1e-9);

  return report("plant economics");
}
