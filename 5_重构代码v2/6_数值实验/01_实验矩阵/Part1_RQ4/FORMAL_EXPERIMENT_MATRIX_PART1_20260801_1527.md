# 正式实验矩阵：Part 1 算法验证与规模表现

- **文档状态：**FINAL — Part 1已冻结
- **生成时间：**2026-08-01 15:27 +08:00
- **对应研究问题：**RQ4
- **代码提交：**`0dd725354732f9b8011e9e7e8540e0e64a3ff223`
- **Git Tag：**`paper-exp-baseline-20260801`
- **全局配置依据：**
  - `FORMAL_BASELINE_FINAL_20260801_1457.yaml`
  - `EXPERIMENT_PROTOCOL_FINAL_20260801_1457.md`

本文件只冻结Part 1的实验专属设计。除本文件明确覆盖的字段外，全部采用已经通过整体验收的正式全局基准。

## 1. 统一随机重复

所有合成实验使用以下10个统一`random_seed`：

```text
101, 202, 303, 404, 505,
606, 707, 808, 909, 1010
```

同一对照中的不同方法必须使用相同实例和相同`random_seed`。当前实现的单个`random_seed`同时控制实例生成和GA随机过程。

# A1：固定减排率下CPLEX与BP正确性对照

## A1.1 目的

验证给定减排率向量后，Branch-and-Price得到的认证最优结果是否与完整列枚举整数主问题的CPLEX最优结果一致。A1只验证内层求解器，不评价外层GA。

## A1.2 规模与固定减排率

规模：

```text
3×5
3×10
5×10
```

固定减排率：

```text
w_j = 0.0,  for all j
w_j = 0.4,  for all j
w_j = 0.8,  for all j
```

固定`w`通过固定减排率入口传入，不受GA染色体编码精度限制。

## A1.3 运行数量

3个规模 × 3组固定w × 10个种子 = 90个基础对照。

每组分别运行CPLEX完整列模型和BP，因此共180次方法运行。

## A1.4 公平性与输出

两种方法必须使用相同实例、固定`w`、模型参数、投资成本口径和数值容差。

每组记录：

- CPLEX与BP目标值；
- 绝对和相对目标差；
- 求解状态与gap；
- 开放配送中心、零售商分配和价格决策；
- 墙钟时间；
- BP的CG迭代数与分支节点数；
- 是否存在多个等价最优方案。

通过标准：CPLEX与BP的目标差不超过正式数值容差。若目标值一致但决策不同，应核查是否属于多个等价最优方案。

# A2：Grid Search–BP与GA–BP的解质量和计算效率比较

## A2.1 目的

在一个可完整枚举但已具有明显计算负担的共同减排率离散空间中，用Grid Search–BP获得确定性最优值，并检验GA–BP能否以更少的BP评价获得相同或接近的解。

## A2.2 正式规模与共同离散空间

正式规模：

```text
5×20
```

A2专用覆盖：

```yaml
chromosome_length: 3
```

每个配送中心的减排率候选集合：

```text
0, 1/7, 2/7, 3/7, 4/7, 5/7, 6/7, 1
```

完整Grid组合数：

```text
8^5 = 32768
```

Grid Search–BP与GA–BP必须使用完全相同的8水平空间。3位编码只用于A2；A3和RQ1—RQ3仍使用正式10位编码。

## A2.3 Grid Search–BP

对每个实例：

1. 穷举全部32768个`w`向量；
2. 每个`w`使用同一正式BP评价器；
3. 取全部认证结果中的最大目标值；
4. 保存最优`w`、网络决策、墙钟时间和BP调用数。

Grid Search–BP对每个固定实例运行一次。

## A2.4 GA–BP

每个实例使用对应统一`random_seed`运行一次GA–BP。除`chromosome_length=3`外，沿用正式GA参数：

```yaml
population_size: 30
max_generation: 50
early_stop: true
convergence_generations: 10
convergence_tolerance: 0.001
parallel_fitness: true
num_threads: 8
```

## A2.5 正式运行前的计时门槛

正式10组A2开始前，先对`random_seed=101`完成一次5×20、32768组合的Grid计时预检。该预检不进入正式统计。

若完整Grid无法在可接受的服务器资源与时间范围内完成，则：

1. 保留失败或超时预检记录；
2. A2整体降为备用规模`4×20`；
3. 保持相同8水平网格，此时组合数为4096；
4. 在任何正式A2运行前建立新的带时间戳矩阵版本；
5. 不得混合5×20与4×20结果。

## A2.6 指标

每组记录：

- Grid最优目标值；
- GA最好目标值；
- GA相对Grid的离散空间optimality gap；
- GA是否找到Grid最优值；
- Grid与GA墙钟时间；
- Grid与GA实际BP调用数；
- GA适应度请求数、缓存命中数和命中率；
- GA达到最终最好值的代数和评价次数；
- 最优`w`与网络决策；
- 所有BP认证状态。

Gap定义为：(Grid最优值 - GA最好值) / Grid最优值绝对值 × 100%。

评价节省比例定义为：1 - GA实际BP调用数 / Grid实际BP调用数。

10组汇总报告成功率、平均/标准差/最大gap、平均墙钟时间、平均BP调用数和平均评价节省比例。

A2只能证明GA–BP在共同3位离散空间中的解质量，不能据此宣称一般连续空间全局最优。

# A3：GA–BP规模扩展实验

## A3.1 正式规模

```text
3×5
3×10
5×10
5×20
5×30
10×30
15×30
15×50
20×50
30×100
```

每个规模使用10个正式`random_seed`，共100次完整GA–BP运行。

## A3.2 使用参数

A3采用正式全局配置：

```yaml
chromosome_length: 10
delta: 3000.0
investment_exponent: 0.5
population_size: 30
max_generation: 50
early_stop: true
convergence_generations: 10
convergence_tolerance: 0.001
pricing_algorithm: 0
pricing_max_cols_per_dc: 3
dual_smooth_alpha: 0
max_cg_iterations: 2000
max_branch_nodes: 10000
bp_time_limit_sec: 600
bp_relative_gap: 0
parallel_fitness: true
num_threads: 8
parallel_pricing: false
cplex_threads: 1
```

按规模从小到大执行；每个规模的10组全部检查后再进入下一规模。30×100不得使用此前固定`w=0.8`冒烟结果替代。

## A3.3 每次运行记录

- 最终最好目标值与整体状态；
- GA实际代数与终止原因；
- 墙钟时间；
- 适应度请求数与实际BP调用数；
- 缓存命中数和命中率；
- 认证最优、非认证可行、无整数解和失败BP数量；
- 总CG迭代与平均每次BP的CG迭代；
- 总分支节点；
- 超时或异常状态；
- 最终减排率与关键网络指标。

每个规模汇总：

- 完成数/10；
- 目标值均值和标准差；
- 墙钟时间均值、最小值和最大值；
- 平均GA代数、BP调用数和缓存命中率；
- 内层BP认证率；
- 平均CG迭代与分支节点；
- 超时数与异常数。

没有已知全局最优值的大规模实例不得报告“optimality gap”。

# 2. Part 1总运行量

在A2保持5×20时：

- A1：90个基础对照，180次方法运行；
- A2：10个Grid Search–BP和10个GA–BP正式运行；
- A3：100次完整GA–BP运行；
- A2计时预检：1次，不进入正式统计。

# 3. 目录结构

```text
results/paper_experiments/
└── part1_algorithm_validation/
    ├── A1_cplex_vs_bp/
    ├── A2_grid_vs_ga_bp/
    │   ├── precheck_seed101/
    │   └── formal/
    └── A3_scalability/
```

# 4. 冻结结论

以下内容已经冻结：

- A1的3个规模、3个固定`w`水平和10个正式种子；
- A2主规模5×20、3位共同离散空间、8个减排率水平和32768个组合；
- A2备用规模4×20及触发规则；
- A3的10个递增规模及每个规模10个正式种子；
- 所有比较指标、状态判定和异常处理规则。

除A2预检触发已写明的版本更新外，Part 1正式运行开始后不得临时改变上述设计。
