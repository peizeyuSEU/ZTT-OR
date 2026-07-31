# 正式实验配置审计

审计基线：`82a9c08`，2026-07-31。

## 固定 w 精确 BP 白名单

下列配置显式满足当前论文口径：

- `pricing_algorithm: 1`（Algorithm 3）
- `investment_exponent: 0.5`
- `cg_early_stop: false`
- `bp_relative_gap: 0`

可用于正式精确实验：

- `release_exact_baseline_10x30.yaml`
- `release_exact_baseline_20x70.yaml`
- `release_exact_baseline_30x100.yaml`
- `release_ablation_30x100_w08_k_parallel.yaml`
- `accelerated_exact_bp_40x150_k1.yaml`
- `verify_parallel_pricing_30x100_w08.yaml`

固定 w 的推荐资源配置为 Algorithm 3、K=3、8线程 DC 并行定价。若触发时间、
节点或CG迭代上限，必须报告最好可行解、有效上界和gap，不能标记为最优。

## 历史配置

以下文件名称包含 `exact`、`baseline` 或 `verify`，但没有显式写入当前完整精确
口径，或仍继承历史默认定价算法。它们仅用于复现旧诊断，不能直接进入论文正式
精确结果表：

- `exact_baseline_3x10_w.yaml`
- `exact_baseline_5x20_w.yaml`
- `exact_baseline_10x30_w.yaml`
- `exact_baseline_20x70_w.yaml`
- `exact_baseline_30x100_w.yaml`
- `exact_large_40x150_w.yaml`
- `verify.yaml`
- `verify_adaptive_30x100_w08.yaml`
- `verify_adaptive_k_30x100_w08.yaml`
- `verify_cache_fix_3x10_w08.yaml`
- `verify_cache_fix_30x100_w08.yaml`
- `verify_fresh_multicol3_3x10_w.yaml`
- `verify_fresh_multicol3_30x100_w08.yaml`
- `verify_hybrid_30x100_w08.yaml`
- `verify_per_dc_k_30x100_w08.yaml`
- `table4_1.yaml`
- `table4_2.yaml`

保留这些文件是为了维持旧结果可复现性；不得通过批量改写把旧配置伪装成新实验。

## 完整 GA

完整 GA 是启发式外层。正式配置必须额外保存：

- 适应度请求数、唯一染色体数和缓存命中数；
- 实际执行的固定 w BP 数；
- 获得最优认证、有可行解但未认证、无整数解的BP数量；
- fork/pipe等适应度评估失败数；
- 最终最好解对应的内层状态、有效上界和gap。

GA外层并行时，内层定价保持串行，避免嵌套超额并行。
