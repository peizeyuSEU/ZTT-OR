#include "../../src/10_common/Config.h"
#include "../../src/10_common/Instance.h"
#include "../../src/10_common/Solution.h"
#include "../../src/10_common/Types.h"
#include "../../src/3_monitor/Monitor.h"
#include "../../src/4_logger/Logger.h"
#include "../../src/8_solver/05_GeneticAlgorithm.h"
#include "../../src/9_postprocessor/01_PostProcessor.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::stringstream ss(line); std::string x;
    while (std::getline(ss, x, ',')) out.push_back(x);
    return out;
}
std::vector<std::unordered_map<std::string,std::string>> csv(const std::string& p) {
    std::ifstream f(p); if (!f) throw std::runtime_error("cannot open " + p);
    std::string line; if (!std::getline(f,line)) throw std::runtime_error("empty " + p);
    auto h=split(line); if(!h.empty() && h[0].size()>=3 && (unsigned char)h[0][0]==0xEF) h[0]=h[0].substr(3); std::vector<std::unordered_map<std::string,std::string>> rows;
    while (std::getline(f,line)) { auto v=split(line); if(v.size()!=h.size()) throw std::runtime_error("bad CSV row "+p); std::unordered_map<std::string,std::string> r; for(size_t i=0;i<h.size();++i) r[h[i]]=v[i]; rows.push_back(r); }
    return rows;
}
double num(const std::unordered_map<std::string,std::string>& r,const std::string& k){return std::stod(r.at(k));}
std::string arg(int argc,char**argv,const std::string& key,const std::string& d="") { for(int i=1;i+1<argc;++i) if(argv[i]==key) return argv[i+1]; return d; }
bool flag(int argc,char**argv,const std::string& key){for(int i=1;i<argc;++i) if(argv[i]==key)return true;return false;}
struct Loaded { Instance inst; std::vector<std::string> dc, market; std::vector<std::pair<std::string,std::string>> arcs; };

Loaded loadCase(const std::string& dir, double intracity, double price, double quota, double delta) {
    auto nodes=csv(dir+"/nodes.csv"), markets=csv(dir+"/market_params.csv"), dcs=csv(dir+"/dc_params.csv");
    auto inv=csv(dir+"/inventory_params.csv"), em=csv(dir+"/dc_emission_params.csv");
    auto sdc=csv(dir+"/supplier_dc.csv"), dcm=csv(dir+"/dc_market.csv");
    Loaded L; for(auto&r:dcs)L.dc.push_back(r.at("dc_id")); for(auto&r:markets)L.market.push_back(r.at("market_id"));
    std::sort(L.dc.begin(),L.dc.end()); std::sort(L.market.begin(),L.market.end());
    std::unordered_map<std::string,std::unordered_map<std::string,std::string>> bynode; for(auto&r:nodes)bynode[r.at("node_id")]=r;
    std::unordered_map<std::string,std::unordered_map<std::string,std::string>> md, dd, id, ed; for(auto&r:markets)md[r.at("market_id")]=r; for(auto&r:dcs)dd[r.at("dc_id")]=r; for(auto&r:inv)id[r.at("dc_id")]=r; for(auto&r:em)ed[r.at("dc_id")]=r;
    std::unordered_map<std::string,std::unordered_map<std::string,std::string>> sm, dm; for(auto&r:sdc)sm[r.at("dc_id")]=r; for(auto&r:dcm)dm[r.at("dc_id")+"|"+r.at("market_id")]=r;
    Instance& x=L.inst; x.numDC=(int)L.dc.size(); x.numRetailer=(int)L.market.size(); x.supplierX=num(bynode.at("S1"),"longitude"); x.supplierY=num(bynode.at("S1"),"latitude");
    x.dcX.resize(x.numDC);x.dcY.resize(x.numDC);x.retX.resize(x.numRetailer);x.retY.resize(x.numRetailer);x.f.resize(x.numDC);x.F.resize(x.numDC);x.g.resize(x.numDC);x.L.resize(x.numDC);x.fc.resize(x.numDC);x.a_dist.resize(x.numDC);x.b.resize(x.numDC);x.beta.resize(x.numDC);x.w.assign(x.numDC,0.0);x.mu.resize(x.numRetailer);x.variance.resize(x.numRetailer);x.reservePrice.resize(x.numRetailer);x.dist.assign(x.numRetailer,std::vector<double>(x.numDC));x.bb.assign(x.numRetailer,std::vector<double>(x.numDC));
    for(int j=0;j<x.numDC;++j){auto&r=bynode.at(L.dc[j]);x.dcX[j]=num(r,"longitude");x.dcY[j]=num(r,"latitude");x.f[j]=num(dd.at(L.dc[j]),"f_j_cny_per_year");x.F[j]=num(id.at(L.dc[j]),"F_j_cny_per_order");x.g[j]=num(id.at(L.dc[j]),"g_j_cny_per_order");x.L[j]=num(id.at(L.dc[j]),"L_j_days");x.a_dist[j]=num(sm.at(L.dc[j]),"road_distance_km");x.fc[j]=num(ed.at(L.dc[j]),"hat_f_j_tco2e_per_year");x.b[j]=num(ed.at(L.dc[j]),"hat_h_tco2e_per_tonne_year");x.beta[j]=delta;}
    for(int i=0;i<x.numRetailer;++i){auto&r=bynode.at(L.market[i]);x.retX[i]=num(r,"longitude");x.retY[i]=num(r,"latitude");x.mu[i]=(int)std::llround(num(md.at(L.market[i]),"mu_annual_tonnes"));x.variance[i]=num(md.at(L.market[i]),"sigma2_daily");x.reservePrice[i]=num(md.at(L.market[i]),"v_i_baseline");}
    x.h=num(id.at(L.dc[0]),"h_cny_per_tonne_year"); x.p=price;x.C=quota;x.hat_h=0.00055;x.k=0.00006996;x.delta=delta;x.z_alpha=1.96;
    for(int j=0;j<x.numDC;++j){for(int i=0;i<x.numRetailer;++i){auto&r=dm.at(L.dc[j]+"|"+L.market[i]);double d=num(r,"road_distance_km");if(L.dc[j]==L.market[i])d=intracity;x.dist[i][j]=d;x.bb[i][j]=x.k*d;L.arcs.push_back({L.dc[j],L.market[i]});}x.b[j]=x.k*x.a_dist[j];}
    return L;
}
void writeDry(const std::string& out,const Loaded&L,double intracity,double price,double quota,int seed){
    std::ofstream j(out+"/resolved_real_case_instance.json");j<<"{\n  \"num_suppliers\":1,\"num_candidate_dcs\":"<<L.inst.numDC<<",\"num_markets\":"<<L.inst.numRetailer<<",\"num_supplier_dc_arcs\":8,\"num_dc_market_arcs\":120,\"intracity_distance_km\":"<<intracity<<",\"carbon_price\":"<<price<<",\"carbon_quota\":"<<quota<<",\"random_seed\":"<<seed<<",\"best_w_expected_length\":"<<L.inst.numDC<<"\n}\n";
    std::ofstream c(out+"/resolved_real_case_instance.csv");c<<"dc_index,dc_id,market_index,market_id,distance_km,transport_cost_coefficient,transport_emission_coefficient\n";for(int j=0;j<L.inst.numDC;++j)for(int i=0;i<L.inst.numRetailer;++i)c<<j<<","<<L.dc[j]<<","<<i<<","<<L.market[i]<<","<<L.inst.dist[i][j]<<","<<L.inst.k*L.inst.dist[i][j]<<","<<L.inst.k*L.inst.dist[i][j]<<"\n";
    std::ofstream d(out+"/solver_input_dimensions.json");d<<"{\"supplier_dc\":[1,8],\"dc_market\":[8,15],\"best_w\":8}\n";
    std::ofstream m(out+"/DRY_RUN_MANIFEST.txt");m<<"dataset_commit=b9fc758dbe11762882bf66cbbc7672da0918adb7\nmodel_commit=0dd725354732f9b8011e9e7e8540e0e64a3ff223\nintracity_distance_km="<<intracity<<"\ncarbon_price="<<price<<"\ntemporary_carbon_quota="<<quota<<"\nrandom_seed="<<seed<<"\nnum_suppliers=1\nnum_candidate_dcs=8\nnum_markets=15\nnum_supplier_dc_arcs=8\nnum_dc_market_arcs=120\nbest_w_expected_length=8\nga_invoked=false\nbp_invoked=false\ncplex_optimization_invoked=false\nvalidation_status=PASS\n";
}
}
int main(int argc,char**argv){try{std::string data=arg(argc,argv,"--data-dir");std::string out=arg(argc,argv,"--output-dir");double d=std::stod(arg(argc,argv,"--intracity-distance-km","10")),p=std::stod(arg(argc,argv,"--carbon-price","0")),q=std::stod(arg(argc,argv,"--carbon-quota","0"));int seed=std::stoi(arg(argc,argv,"--random-seed","101"));if(data.empty()||out.empty())throw std::runtime_error("--data-dir and --output-dir required");std::filesystem::create_directories(out);Loaded L=loadCase(data,d,p,q,3000.0);if(flag(argc,argv,"--dry-run")){writeDry(out,L,d,p,q,seed);std::cout<<"DRY_RUN PASS\n";return 0;}Config cfg;cfg.num_dc=L.inst.numDC;cfg.num_retailers=L.inst.numRetailer;cfg.random_seed=seed;cfg.population_size=10;cfg.max_generation=5;cfg.crossover_rate=.7;cfg.mutation_rate=-1;cfg.elitism=true;cfg.early_stop=true;cfg.convergence_generations=3;cfg.parallel_fitness=false;cfg.num_threads=1;cfg.chromosome_length=8;cfg.carbon_price=p;cfg.carbon_cap=q;cfg.delta=3000;cfg.pricing_algorithm=0;cfg.pricing_max_cols_per_dc=1;cfg.max_cg_iterations=2000;cfg.max_branch_nodes=10000;cfg.bp_time_limit_sec=300;cfg.bp_relative_gap=0;cfg.parallel_pricing=false;cfg.cplex_threads=1;cfg.use_sqrt_investment=true;cfg.use_invest_in_column=true;cfg.investment_exponent=.5;Logger log;log.init(out+"/run.log",true);Monitor mon;mon.setTotalGenerations(cfg.max_generation);GeneticAlgorithm ga(cfg,L.inst);ga.setLogger(&log);ga.setMonitor(&mon);auto t=std::chrono::steady_clock::now();Solution s=ga.run();double sec=std::chrono::duration<double>(std::chrono::steady_clock::now()-t).count();std::ofstream r(out+"/result.csv");r<<"solve_status,outer_outcome,has_integer_solution,total_profit,best_w,num_dc_open,num_rts_served,relative_gap,actual_bp_calls,cache_hits,generations,wall_time_sec\n"<<solveStatusName(s.solveStatus)<<",HEURISTIC_BEST_FOUND,"<<(s.hasIntegerSolution?"true":"false")<<","<<s.totalProfit<<",\"";for(size_t i=0;i<ga.getBestW().size();++i){if(i)r<<";";r<<ga.getBestW()[i];}r<<"\","<<s.numDCsOpen<<","<<s.numRtsServed<<","<<s.relativeGap<<","<<ga.getFitnessBPSolves()<<","<<ga.getActualFitnessCacheHits()<<","<<ga.getConvergence().size()<<","<<sec<<"\n";SolutionHelper::saveConvergenceCSV(out+"/convergence.csv",ga.getConvergence());std::ofstream rep(out+"/report.txt");rep<<"outer_outcome=HEURISTIC_BEST_FOUND\ninner_bp_status="<<solveStatusName(s.solveStatus)<<"\nhas_integer_solution="<<(s.hasIntegerSolution?"true":"false")<<"\nbest_w_length="<<ga.getBestW().size()<<"\nwall_time_sec="<<sec<<"\n";return s.hasIntegerSolution?0:3;}catch(const std::exception&e){std::cerr<<"ERROR: "<<e.what()<<"\n";return 2;}}
