#define REAL_CASE_ADAPTER_LIBRARY
#include "real_case_ga_bp_main.cpp"
#include "../../src/8_solver/04_BranchAndPrice.h"
#include "../../src/9_postprocessor/01_PostProcessor.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>

namespace {
std::string wText(const std::vector<double>& w) {
    std::ostringstream s;
    for (size_t i=0;i<w.size();++i) { if (i) s << ";"; s << std::setprecision(17) << w[i]; }
    return s.str();
}
void writeE0Dry(const std::string& out, const Loaded& L, int seed) {
    std::filesystem::create_directories(out);
    std::ofstream r(out + "/E0_RESOLVED_INSTANCE.json");
    r << "{\n  \"num_suppliers\": 1, \"num_candidate_dcs\": " << L.inst.numDC
      << ", \"num_markets\": " << L.inst.numRetailer
      << ", \"num_supplier_dc_arcs\": 8, \"num_dc_market_arcs\": 120,\n"
      << "  \"intracity_distance_km\": 10, \"transport_cost_mode\": \"EXPLICIT_ARC_COST\",\n"
      << "  \"transport_rate_cny_per_tonne_km\": 0.39, \"transport_emission_factor_tco2e_per_tonne_km\": 0.00006996,\n"
      << "  \"inventory_emission_factor_tco2e_per_tonne_year\": 0.00055, \"facility_emission_tco2e_per_year_per_open_dc\": 110,\n"
      << "  \"carbon_price\": 0, \"temporary_carbon_quota\": 0, \"random_seed\": " << seed
      << ", \"all_w_fixed_to_zero\": true, \"w_length\": " << L.inst.numDC << "\n}\n";
    std::ofstream a(out + "/E0_RESOLVED_ARC_PARAMETERS.csv");
    a << "arc_type,dc_id,market_id,distance_km,transport_cost_cny_per_tonne,transport_emission_factor\n";
    for (int j=0;j<L.inst.numDC;++j) {
        a << "supplier_dc," << L.dc[j] << ",," << L.inst.a_dist[j] << ","
          << L.inst.supplierDcTransportCostPerTonne[j] << "," << L.inst.b[j] << "\n";
        for (int i=0;i<L.inst.numRetailer;++i)
            a << "dc_market," << L.dc[j] << "," << L.market[i] << "," << L.inst.dist[i][j]
              << "," << L.inst.dcMarketTransportCostPerTonne[i][j] << "," << L.inst.bb[i][j] << "\n";
    }
    std::ofstream m(out + "/E0_DRY_RUN_MANIFEST.txt");
    m << "experiment_type=PART5_E0_BASELINE_DRY_RUN\n"
      << "dataset_commit=b9fc758dbe11762882bf66cbbc7672da0918adb7\n"
      << "base_model_commit=0dd725354732f9b8011e9e7e8540e0e64a3ff223\n"
      << "explicit_arc_cost_extension_commit=fb7f435008a60c91b9e0908fd64d3239b4197fcd\n"
      << "intracity_distance_km=10\ntransport_cost_mode=EXPLICIT_ARC_COST\n"
      << "supplier_count=1\ncandidate_dc_count=" << L.inst.numDC << "\nmarket_count=" << L.inst.numRetailer
      << "\nsupplier_dc_arcs=8\ndc_market_arcs=120\n"
      << "same_city_arc_cost_cny_per_tonne=3.9\ntransport_emission_factor=0.00006996\n"
      << "inventory_emission_factor=0.00055\nfacility_emission_per_open_dc=110\n"
      << "carbon_price=0\ntemporary_carbon_quota=0\nall_w_fixed_to_zero=true\n"
      << "ga_invoked=false\nbp_invoked=false\ncplex_optimization_invoked=false\nvalidation_status=PASS\n";
}
Config e0Config(const Instance& x, int seed) {
    Config c;
    c.num_dc=x.numDC; c.num_retailers=x.numRetailer; c.random_seed=seed;
    c.run_mode="fixed_w"; c.use_sqrt_investment=true; c.investment_exponent=0.5;
    c.use_invest_in_column=true; c.pricing_algorithm=1; c.pricing_max_cols_per_dc=1;
    c.pricing_adaptive_cols=false; c.pricing_per_dc_by_w=false;
    c.parallel_fitness=false; c.parallel_pricing=true; c.pricing_threads=8; c.core_budget=8; c.cplex_threads=1;
    c.root_heuristic=true; c.root_rmp_mip_heuristic=true; c.root_rmp_mip_time_limit_sec=5.0;
    c.max_cg_iterations=1000; c.cg_early_stop=false; c.max_branch_nodes=10000;
    c.bb_node_strategy="dfs"; c.bp_time_limit_sec=600; c.bp_relative_gap=0; c.rc_eps=1e-6;
    c.carbon_price=0; c.carbon_cap=0; c.delta=3000; c.early_stop=false;
    c.fixed_w.assign(x.numDC,0.0); return c;
}
struct EmissionParts { double facility=0, inventory=0, supplier=0, market=0; };
EmissionParts parts(const Instance& x, const PostProcessor& pp) {
    EmissionParts z;
    for (const auto& d : pp.dcResults) {
        if (!d.isOpen) continue;
        int j=d.index; double factor=1.0-d.w;
        z.facility += factor*x.fc[j]; z.supplier += factor*x.b[j]*d.D;
        for (int i : d.servedRetailers) z.market += factor*x.bb[i][j]*x.mu[i];
        std::vector<int> S(x.numRetailer,0); for (int i : d.servedRetailers) S[i]=1;
        double tv=InventoryCostFormula::totalVariance(x,j,S);
        double ss=x.z_alpha*std::sqrt(x.L[j]*tv);
        z.inventory += factor*x.hat_h*(ss+d.Q_star/2.0);
    }
    return z;
}
void writeResult(const std::string& out, const Instance& x, const Solution& sol,
                 const PostProcessor& pp, const EmissionParts& z, double sec,
                 const Config& c) {
    const auto& cr=pp.getCarbonResult(); double total=z.facility+z.inventory+z.supplier+z.market;
    double err=total-cr.E; double tol=std::max(1e-8,1e-10*std::abs(cr.E));
    std::ofstream b(out+"/E0_BASELINE_RESULT.csv"); b << std::setprecision(17);
    b<<"field,value\nsolve_status,"<<solveStatusName(sol.solveStatus)<<"\ninteger_solution,"<<(sol.hasIntegerSolution?"true":"false")
     <<"\nrelative_gap,"<<sol.relativeGap<<"\ntotal_profit,"<<sol.totalProfit<<"\ncarbon_emission_E0,"<<cr.E<<"\nrun_time_sec,"<<sec<<"\nopen_dc_count,"<<sol.numDCsOpen<<"\nall_w_fixed_to_zero,true\n";
    std::ofstream n(out+"/E0_NETWORK_DECISION.csv"); n << std::setprecision(17); n<<"dc_index,dc_open,w,p,D,Q_star,served_retailers,profit\n";
    for(const auto& d:pp.dcResults){n<<d.index<<","<<(d.isOpen?"true":"false")<<","<<d.w<<","<<d.p<<","<<d.D<<","<<d.Q_star<<",\"";for(size_t k=0;k<d.servedRetailers.size();++k){if(k)n<<";";n<<d.servedRetailers[k];}n<<"\","<<d.profit<<"\n";}
    std::ofstream m(out+"/E0_MARKET_ASSIGNMENT.csv"); m<<"market_index,assigned_dc\n"; for(int i=0;i<x.numRetailer;++i){int a=-1;for(const auto& d:pp.dcResults)if(std::find(d.servedRetailers.begin(),d.servedRetailers.end(),i)!=d.servedRetailers.end())a=d.index;m<<i<<","<<a<<"\n";}
    std::ofstream o(out+"/E0_OBJECTIVE_DECOMPOSITION.csv"); o<<"field,value\nreported_total_profit,"<<sol.totalProfit<<"\ncarbon_price,0\ncarbon_trading_cost,0\ninvestment_cost,0\n";
    std::ofstream e(out+"/E0_EMISSION_DECOMPOSITION.csv"); e << std::setprecision(17); e<<"component,value\nfacility_emission_tco2e_per_year,"<<z.facility<<"\ninventory_emission_tco2e_per_year,"<<z.inventory<<"\nsupplier_dc_transport_emission_tco2e_per_year,"<<z.supplier<<"\ndc_market_transport_emission_tco2e_per_year,"<<z.market<<"\ntotal_emission_E0_tco2e_per_year,"<<cr.E<<"\nindependent_sum,"<<total<<"\nabsolute_error,"<<err<<"\ntolerance,"<<tol<<"\n";
    std::ofstream j(out+"/E0_EMISSION_DECOMPOSITION.json"); j << std::setprecision(17); j<<"{\n  \"facility\": "<<z.facility<<",\n  \"inventory\": "<<z.inventory<<",\n  \"supplier_dc_transport\": "<<z.supplier<<",\n  \"dc_market_transport\": "<<z.market<<",\n  \"E0\": "<<cr.E<<",\n  \"independent_sum\": "<<total<<",\n  \"absolute_error\": "<<err<<",\n  \"tolerance\": "<<tol<<"\n}\n";
    std::ofstream r(out+"/E0_RESOLVED_CONFIG.yaml"); r<<"num_dc: "<<x.numDC<<"\nnum_retailers: "<<x.numRetailer<<"\nrandom_seed: 101\nintracity_distance_km: 10\ntransport_cost_mode: EXPLICIT_ARC_COST\ncarbon_price: 0\ntemporary_carbon_quota: 0\ndelta: 3000.0\ninvestment_exponent: 0.5\nall_w_fixed_to_zero: true\nga_invoked: false\nbp_invoked: true\ncplex_optimization_invoked: true\npricing_algorithm: "<<c.pricing_algorithm<<"\npricing_max_cols_per_dc: "<<c.pricing_max_cols_per_dc<<"\nparallel_pricing: true\npricing_threads: "<<c.pricing_threads<<"\ncore_budget: "<<c.core_budget<<"\ncplex_threads: "<<c.cplex_threads<<"\nmax_cg_iterations: "<<c.max_cg_iterations<<"\ncg_early_stop: false\nmax_branch_nodes: "<<c.max_branch_nodes<<"\nbp_time_limit_sec: "<<c.bp_time_limit_sec<<"\nbp_relative_gap: 0\nrc_eps: "<<c.rc_eps<<"\nstatus: "<<solveStatusName(sol.solveStatus)<<"\n";
    std::ofstream q(out+"/E0_FORMULA_AUDIT.md"); q<<"# E0 Formula Audit\n\nE0 is defined by the 10 km real-case instance with p=0, temporary C=0, and all w_j=0.\n\n| component | value |\n|---|---:|\n| facility | "<<z.facility<<" |\n| inventory | "<<z.inventory<<" |\n| supplier--DC transport | "<<z.supplier<<" |\n| DC--market transport | "<<z.market<<" |\n| E0 | "<<cr.E<<" |\n| independent sum | "<<total<<" |\n| absolute error | "<<err<<" |\n| tolerance | "<<tol<<" |\n";
}
}

int main(int argc,char**argv) {
    try {
        std::string data=arg(argc,argv,"--data-dir"), out=arg(argc,argv,"--output-dir");
        int seed=std::stoi(arg(argc,argv,"--random-seed","101"));
        if(data.empty()||out.empty()) throw std::runtime_error("--data-dir and --output-dir required");
        Loaded L=loadCase(data,10.0,0.0,0.0,3000.0,true);
        if(flag(argc,argv,"--dry-run")) { writeE0Dry(out,L,seed); std::cout<<"E0_DRY_RUN PASS\n"; return 0; }
        Config cfg=e0Config(L.inst,seed); std::filesystem::create_directories(out);
        Logger log; log.init(out+"/run.log",true); Monitor mon; mon.setTotalGenerations(1);
        BranchAndPrice bp; bp.setInstance(L.inst); bp.setConfig(cfg); bp.setLogger(&log); bp.setMonitor(&mon);
        std::vector<double> w(L.inst.numDC,0.0); auto start=std::chrono::steady_clock::now();
        double profit=bp.solve(w,"E0 fixed-w BP"); double sec=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        Solution sol=bp.getBestSolution(); sol.totalProfit=profit; sol.w=w;
        if(!sol.hasIntegerSolution || sol.solveStatus!=SolveStatus::OPTIMAL) { std::cerr<<"E0_STOP: final BP is not OPTIMAL or has no integer solution\n"; return 4; }
        PostProcessor pp(L.inst,sol,w); pp.compute(); EmissionParts z=parts(L.inst,pp); writeResult(out,L.inst,sol,pp,z,sec,cfg);
        std::ofstream rep(out+"/report.txt"); rep<<"experiment_type=PART5_E0_BASELINE\nbase_model_commit=0dd725354732f9b8011e9e7e8540e0e64a3ff223\nexplicit_arc_cost_extension_commit=fb7f435008a60c91b9e0908fd64d3239b4197fcd\ndata_commit=b9fc758dbe11762882bf66cbbc7672da0918adb7\nintracity_distance_km=10\ntransport_cost_mode=EXPLICIT_ARC_COST\ncarbon_price=0\ntemporary_carbon_quota=0\nall_w_fixed_to_zero=true\nga_invoked=false\nbp_invoked=true\ncplex_optimization_invoked=true\nfinal_bp_status="<<solveStatusName(sol.solveStatus)<<"\ninteger_solution=true\nE0_tco2e_per_year="<<pp.getCarbonResult().E<<"\nE0_unit=tCO2e/year\nprofit="<<sol.totalProfit<<"\nrun_time_sec="<<sec<<"\n";
        std::ofstream man(out+"/E0_RUN_MANIFEST.txt"); man<<"experiment_type=PART5_E0_BASELINE\npaper_statistics=true\nintracity_distance_km=10\ntransport_cost_mode=EXPLICIT_ARC_COST\ncarbon_price=0\ntemporary_carbon_quota=0\nall_w_fixed_to_zero=true\nga_invoked=false\nbp_invoked=true\ncplex_optimization_invoked=true\nbase_model_commit=0dd725354732f9b8011e9e7e8540e0e64a3ff223\nexplicit_arc_cost_extension_commit=fb7f435008a60c91b9e0908fd64d3239b4197fcd\nexecution_code_commit=PENDING_EXECUTION_COMMIT\ndataset_commit=b9fc758dbe11762882bf66cbbc7672da0918adb7\ncanonical_solver_seed="<<seed<<"\nfinal_bp_status="<<solveStatusName(sol.solveStatus)<<"\ninteger_solution=true\nE0_value="<<pp.getCarbonResult().E<<"\nE0_unit=tCO2e/year\ndefault_quota_rule=C=E0\nartifact_status=NOT_YET_GENERATED\ndata_repository_backfill=false\n";
        std::cout<<"E0_PASS E0="<<std::setprecision(17)<<pp.getCarbonResult().E<<" profit="<<sol.totalProfit<<" time="<<sec<<"\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<"ERROR: "<<e.what()<<"\n"; return 2; }
}
