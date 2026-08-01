#include "src/10_common/Config.h"
#include "src/6_preprocessor/DataGenerator.h"
#include "src/7_formula/08_Objective.h"
#include "src/7_formula/01_Revenue.h"
#include "src/8_solver/03_BranchAndBound.h"
#include "src/9_postprocessor/01_PostProcessor.h"
#include <ilcplex/ilocplex.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/resource.h>
#include <vector>

ILOSTLBEGIN
namespace fs = std::filesystem;

struct Col { int dc; std::vector<int> S; double p; double profit; IloNumVar var; };

static std::string csv(double x) { std::ostringstream o; o << std::setprecision(17) << x; return o.str(); }
static long peakRssKb() { struct rusage u{}; getrusage(RUSAGE_SELF, &u); return (long)u.ru_maxrss; }

static bool validateSol(const Instance& inst, const Solution& sol) {
    std::vector<int> used(inst.numRetailer, 0);
    std::vector<int> dcUsed(inst.numDC, 0);
    for (const auto& d : sol.dcSolutions) {
        if (d.dcIndex < 0 || d.dcIndex >= inst.numDC || dcUsed[d.dcIndex]++) return false;
        if ((int)d.S.size() != inst.numRetailer || !RevenueFormula::serviceSetAcceptsPrice(inst, d.S, d.p)) return false;
        for (int i = 0; i < inst.numRetailer; ++i) if (d.S[i]) { if (used[i]++) return false; }
    }
    return true;
}

static double completeMaster(const Instance& inst, const Config& cfg, Solution& out,
                             int& colCount, double& wall, double& enumTime,
                             double& buildTime, double& optTime) {
    auto t0 = std::chrono::steady_clock::now();
    auto stage = t0;
    IloEnv env; out = Solution(); colCount = 0;
    try {
        IloModel model(env); IloObjective obj = IloAdd(model, IloMaximize(env));
        IloRangeArray rRows(env, inst.numRetailer, -IloInfinity, 1.0);
        IloRangeArray dRows(env, inst.numDC, -IloInfinity, 1.0);
        model.add(rRows); model.add(dRows); IloNumVarArray vars(env); std::vector<Col> cols;
        ObjectiveFormula formula(cfg);
        for (int j=0;j<inst.numDC;++j) for (double p: inst.reservePrice) for (int mask=1; mask<(1<<inst.numRetailer); ++mask) {
            std::vector<int> S(inst.numRetailer); for(int i=0;i<inst.numRetailer;++i) S[i]=(mask>>i)&1;
            if (!RevenueFormula::serviceSetAcceptsPrice(inst,S,p)) continue;
            IloNumColumn c = obj(formula.columnProfit(inst,j,S,p,inst.w[j])); c += dRows[j](1.0);
            for(int i=0;i<inst.numRetailer;++i) if(S[i]) c += rRows[i](1.0);
            IloNumVar v(c,0.0,1.0,ILOBOOL); vars.add(v); cols.push_back({j,S,p,formula.columnProfit(inst,j,S,p,inst.w[j]),v});
        }
        enumTime=std::chrono::duration<double>(std::chrono::steady_clock::now()-stage).count();
        stage=std::chrono::steady_clock::now(); colCount = (int)cols.size();
        IloCplex cp(model); cp.setOut(env.getNullStream()); cp.setWarning(env.getNullStream());
        buildTime=std::chrono::duration<double>(std::chrono::steady_clock::now()-stage).count();
        cp.setParam(IloCplex::Threads,1); cp.setParam(IloCplex::EpGap,0.0); cp.setParam(IloCplex::EpOpt,1e-9);
        stage=std::chrono::steady_clock::now();
        if(!cp.solve() || cp.getStatus()!=IloAlgorithm::Optimal) throw std::runtime_error("CPLEX_NOT_OPTIMAL");
        optTime=std::chrono::duration<double>(std::chrono::steady_clock::now()-stage).count();
        out.hasIntegerSolution=true; out.solveStatus=SolveStatus::OPTIMAL; out.totalProfit=cp.getObjValue(); out.w=inst.w;
        for(const auto& c: cols) if(cp.getValue(c.var)>0.5) { DCSolution d; d.dcIndex=c.dc; d.S=c.S; d.p=c.p; d.w=inst.w[c.dc]; d.profit=c.profit; out.dcSolutions.push_back(d); }
        env.end(); wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count(); return out.totalProfit;
    } catch(...) { try{env.end();}catch(...){} wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count(); throw; }
}

int main(int argc,char**argv){
    if(argc!=7){std::cerr<<"usage: a1_pair_runner baseline.yaml num_dc num_retailers seed uniform_w output_dir\n";return 2;}
    try{
        Config cfg;
        // Materialize the frozen global baseline explicitly.  The global YAML
        // is a documentation snapshot with nested sections and is intentionally
        // not passed through the legacy flat Config parser.
        cfg.carbon_price=2.0; cfg.carbon_cap=100000.0; cfg.inv_carbon_coeff=2.0;
        cfg.transport_carbon_coeff=0.01; cfg.fac_carbon_ratio=0.1; cfg.delta=3000.0;
        cfg.investment_exponent=0.5; cfg.min_w=0.0; cfg.max_w=1.0; cfg.chromosome_length=10;
        cfg.use_sqrt_investment=true; cfg.use_invest_in_column=true; cfg.transport_direct_distance=false;
        cfg.service_level=0.975; cfg.z_alpha=1.96;
        cfg.coord_min=0.0; cfg.coord_max=1000.0; cfg.supplier_x=500.0; cfg.supplier_y=500.0;
        cfg.mu_min=2000.0; cfg.mu_max=3500.0; cfg.var_min=10.0; cfg.var_max=15.0;
        cfg.fixed_cost_min=7000.0; cfg.fixed_cost_max=10000.0; cfg.reserve_price_min=60.0; cfg.reserve_price_max=80.0;
        cfg.order_fixed_cost=100.0; cfg.transport_fixed_cost=100.0; cfg.lead_time=1.0; cfg.holding_cost=10.0;
        cfg.pricing_algorithm=0; cfg.pricing_max_cols_per_dc=3; cfg.dual_smooth_alpha=0.0;
        cfg.rc_eps=1.0e-6; cfg.max_cg_iterations=2000; cfg.max_branch_nodes=10000;
        cfg.bp_time_limit_sec=600.0; cfg.bp_relative_gap=0.0; cfg.cg_early_stop=false; cfg.use_dfs=true;
        cfg.root_heuristic=true; cfg.root_rmp_mip_heuristic=false; cfg.legacy_mode=false;
        cfg.cplex_threads=1; cfg.parallel_pricing=false; cfg.pricing_threads=4;
        cfg.num_dc=std::stoi(argv[2]); cfg.num_retailers=std::stoi(argv[3]); cfg.random_seed=std::stoi(argv[4]);
        double w=std::stod(argv[5]); cfg.run_mode="fixed_w"; cfg.fixed_w.assign(cfg.num_dc,w);
        cfg.parallel_fitness=false; cfg.num_threads=1; cfg.parallel_pricing=false; cfg.cplex_threads=1;
        cfg.output_dir=argv[6]; fs::create_directories(cfg.output_dir); cfg.saveToFile(cfg.output_dir+"/resolved_config.yaml");
        DataGenerator gen(cfg.random_seed); gen.setLegacyMode(cfg.legacy_mode); Instance inst=gen.generate(cfg); inst.setEmissionReductionRates(cfg.fixed_w);
        std::ofstream f(cfg.output_dir+"/instance_fingerprint.txt"); f<<"num_dc="<<cfg.num_dc<<"\nnum_retailers="<<cfg.num_retailers<<"\nrandom_seed="<<cfg.random_seed<<"\nuniform_w="<<w<<"\n"; f.close();
        Solution ref; int ncols=0; double cplexWall=0, enumTime=0, buildTime=0, optTime=0;
        double c0=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        double cobj=completeMaster(inst,cfg,ref,ncols,cplexWall,enumTime,buildTime,optTime);
        auto rv0=std::chrono::steady_clock::now(); bool refFeas=validateSol(inst,ref);
        double refValTime=std::chrono::duration<double>(std::chrono::steady_clock::now()-rv0).count();
        cplexWall += refValTime;
        long refRss=peakRssKb();
        BranchAndBound bp; auto bi0=std::chrono::steady_clock::now(); bp.setInstance(inst); bp.setConfig(cfg);
        double bpInitTime=std::chrono::duration<double>(std::chrono::steady_clock::now()-bi0).count();
        auto b0=std::chrono::steady_clock::now(); double bobj=bp.solve(); double bpSolveTime=std::chrono::duration<double>(std::chrono::steady_clock::now()-b0).count(); const Solution& bs=bp.getBestSolution();
        auto bv0=std::chrono::steady_clock::now(); bool bpFeas=bs.hasIntegerSolution && validateSol(inst,bs);
        double bpValTime=std::chrono::duration<double>(std::chrono::steady_clock::now()-bv0).count();
        double bpWall=bpInitTime+bpSolveTime+bpValTime;
        double absd=std::abs(cobj-bobj), reld=absd/std::max(1.0,std::abs(cobj));
        long bpRss=peakRssKb();
        std::ofstream r(cfg.output_dir+"/pair_result.csv"); r<<"num_dc,num_retailers,random_seed,fixed_w,complete_column_count,column_enumeration_time_sec,full_master_build_time_sec,cplex_optimization_time_sec,reference_validation_time_sec,reference_total_wall_time_sec,reference_peak_rss_kb,bp_initialization_time_sec,bp_solve_time_sec,bp_validation_time_sec,bp_total_wall_time_sec,bp_peak_rss_kb,bp_initial_columns,bp_generated_columns,bp_final_master_columns,cplex_status,cplex_objective,cplex_gap,bp_status,bp_objective,bp_gap,absolute_objective_difference,relative_objective_difference,reference_solution_feasible,bp_solution_feasible,bp_cg_iterations,bp_branch_nodes,bp_processed_nodes,bp_pruned_nodes,bp_remaining_active_nodes,pair_valid\n";
        bool valid=refFeas && bpFeas && bp.getSolveStatus()==SolveStatus::OPTIMAL && absd<=1e-7*std::max(1.0,std::abs(cobj));
        r<<cfg.num_dc<<","<<cfg.num_retailers<<","<<cfg.random_seed<<","<<csv(w)<<","<<ncols<<","<<csv(enumTime)<<","<<csv(buildTime)<<","<<csv(optTime)<<","<<csv(refValTime)<<","<<csv(cplexWall)<<","<<refRss<<","<<csv(bpInitTime)<<","<<csv(bpSolveTime)<<","<<csv(bpValTime)<<","<<csv(bpWall)<<","<<bpRss<<",UNKNOWN,"<<bp.getCGTotalColumns()<<",UNKNOWN,OPTIMAL,"<<csv(cobj)<<",0,"<<solveOutcomeName(bp.getSolveStatus(),bs.hasIntegerSolution)<<","<<csv(bobj)<<","<<csv(bp.getRelativeGap())<<","<<csv(absd)<<","<<csv(reld)<<","<<(refFeas?"true":"false")<<","<<(bpFeas?"true":"false")<<","<<bp.getCGIterations()<<","<<bp.getTotalNodes()<<","<<bp.getProcessedNodes()<<","<<bp.getPrunedNodes()<<","<<bp.getRemainingActiveNodes()<<","<<(valid?"true":"false")<<"\n"; r.close();
        std::ofstream log(cfg.output_dir+"/run.log"); log<<"complete_columns="<<ncols<<"\ncolumn_enumeration_time_sec="<<csv(enumTime)<<"\nfull_master_build_time_sec="<<csv(buildTime)<<"\ncplex_optimization_time_sec="<<csv(optTime)<<"\nreference_validation_time_sec="<<csv(refValTime)<<"\nreference_total_wall_time_sec="<<csv(cplexWall)<<"\ncplex_status=OPTIMAL\ncplex_objective="<<csv(cobj)<<"\nbp_initialization_time_sec="<<csv(bpInitTime)<<"\nbp_solve_time_sec="<<csv(bpSolveTime)<<"\nbp_validation_time_sec="<<csv(bpValTime)<<"\nbp_total_wall_time_sec="<<csv(bpWall)<<"\nbp_status="<<solveOutcomeName(bp.getSolveStatus(),bs.hasIntegerSolution)<<"\nbp_objective="<<csv(bobj)<<"\nbp_cg_iterations="<<bp.getCGIterations()<<"\nbp_branch_nodes="<<bp.getTotalNodes()<<"\npair_valid="<<(valid?"true":"false")<<"\n"; log.close();
        std::cout<<"PAIR_VALID="<<(valid?"true":"false")<<" CPLEX="<<csv(cobj)<<" BP="<<csv(bobj)<<" ABS="<<csv(absd)<<" COLS="<<ncols<<"\n"; return valid?0:1;
    }catch(const std::exception&e){std::cerr<<"ERROR: "<<e.what()<<"\n";return 1;}
}
