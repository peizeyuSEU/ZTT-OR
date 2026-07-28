#!/bin/sh
# 生成 4规模×4w 共16组单次探底矩阵配置
# 规模: 20x70 30x100 40x150 50x200
# w: 0.0 0.2 0.5 0.8 (min_w=max_w 强制全DC同一w)

CFG_DIR="src/0_experiments/configs"

gen_one() {
    DC="$1"
    R="$2"
    W="$3"
    WTAG="$4"
    FILE="${CFG_DIR}/matrix_${DC}x${R}_w${WTAG}.yaml"
    cat > "$FILE" <<EOF
plan_name: "矩阵 ${DC}x${R} w=${W}"
plan_desc: "${DC}DC x ${R}R 单个体单次求解 固定w=${W}"

num_dc: ${DC}
num_retailers: ${R}
population_size: 1
max_generation: 1
crossover_rate: 0.7
mutation_rate: 0.2
chromosome_length: 10
carbon_price: 2.0
carbon_cap: 100000.0
delta: 3000
holding_cost: 10.0
random_seed: 32501
min_w: ${W}
max_w: ${W}
use_sqrt_investment: true
early_stop: false
parallel_fitness: false
max_cg_iterations: 300
cg_early_stop: true
cg_stall_tolerance: 0.0001
cg_stall_iterations: 15

experiments:
  - name: "S1_Serial"
    parallel_pricing: false
    pricing_threads: 4
    cplex_threads: 1
EOF
    echo "generated: $FILE"
}

for SCALE in "20 70" "30 100" "40 150" "50 200"; do
    set -- $SCALE
    DC="$1"
    R="$2"
    gen_one "$DC" "$R" "0.0" "00"
    gen_one "$DC" "$R" "0.2" "02"
    gen_one "$DC" "$R" "0.5" "05"
    gen_one "$DC" "$R" "0.8" "08"
done