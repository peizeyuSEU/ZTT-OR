# A1正式实验矩阵：固定减排率下CPLEX与BP正确性验证

- **状态：**FINAL — A1已冻结
- **生成时间：**2026-08-01 19:22 +08:00
- **实验编号：**Part 1 / RQ4 / A1
- **算法基准提交：**`0dd725354732f9b8011e9e7e8540e0e64a3ff223`
- **正式标签：**`paper-exp-baseline-20260801`

## 1. 验证目的

A1只验证固定减排率向量给定后，Branch-and-Price是否能够复现完整列整数主问题的CPLEX认证最优目标值。

A1不运行GA，也不用于证明整体GA–BP全局最优。

## 2. 实验设计

规模：

```text
3×5
3×10
5×10
```

统一固定减排率：

```text
w_j=0.0，对所有j
w_j=0.4，对所有j
w_j=0.8，对所有j
```

正式种子：

```text
101, 202, 303, 404, 505,
606, 707, 808, 909, 1010
```

总计：

```text
3个规模 × 3组固定w × 10个种子 = 90组配对
90组 × 2种方法 = 180次方法级运行
```

## 3. 完整列CPLEX口径

参考方法必须：

1. 按当前模型定义，为每个配送中心枚举全部可行服务集合；
2. 使用与BP完全相同的列利润、库存、运输、碳成本和投资成本函数；
3. 建立完整整数主问题；
4. 使用CPLEX求得`OPTIMAL`且gap为0；
5. 不得使用受限主问题、LP松弛或遗漏可行列。

## 4. BP口径

BP使用冻结参数：

```yaml
delta: 3000.0
investment_exponent: 0.5
pricing_algorithm: 0
pricing_max_cols_per_dc: 3
dual_smooth_alpha: 0
rc_eps: 1.0e-6
max_cg_iterations: 2000
max_branch_nodes: 10000
bp_time_limit_sec: 600
bp_relative_gap: 0
parallel_pricing: false
cplex_threads: 1
use_sqrt_investment: true
use_invest_in_column: true
transport_direct_distance: false
cg_early_stop: false
legacy_mode: false
```

A1不使用GA：

```yaml
ga_enabled: false
parallel_fitness: false
num_threads: 1
```

固定`w`直接传入，不经过染色体编码。

## 5. 单组正式前验收

正式90组开始前先运行：

```text
规模：3×5
random_seed：101
固定w：0.4
```

两种方法均须认证最优、目标值在正式容差内一致，且两套解均能代回原模型验证可行。

## 6. 比较规则

每组计算：

```text
absolute_difference = abs(cplex_objective - bp_objective)

relative_difference =
abs(cplex_objective - bp_objective) / abs(cplex_objective)
```

决策向量不要求逐项完全相同。若目标值一致、两套解均可行但决策不同，应标记为等价多最优方案，而不是判定失败。

## 7. 整体通过标准

完整通过要求：

- 90/90组CPLEX均为`OPTIMAL`且gap为0；
- 90/90组BP均为`OPTIMAL_CERTIFIED`且gap为0；
- 90/90组目标值在正式数值容差内一致；
- 两种方法的解均通过原模型可行性检查；
- 不存在配置偏离、完整列遗漏或成本口径差异。

## 8. 论文表述边界

A1通过后只能说明：

> 给定固定减排率向量时，BP在测试实例上复现了完整列整数主问题的认证最优目标值。

不得由A1推出整体GA–BP具有全局最优保证。
