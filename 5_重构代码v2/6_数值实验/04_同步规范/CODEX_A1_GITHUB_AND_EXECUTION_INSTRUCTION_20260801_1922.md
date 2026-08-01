请完成Part 1 / RQ4 / A1的完整流程：

1. 将本压缩包中的A1正式配置同步到GitHub；
2. 做一个3×5、seed=101、w=0.4的单组正式前验收；
3. 验收通过后，运行90组正式CPLEX vs BP配对；
4. 汇总并将正式结果同步回GitHub；
5. 不修改求解器核心代码。

## 一、仓库与基准

仓库：`peizeyuSEU/ZTT-OR`
分支：`master`

正式算法提交：

`0dd725354732f9b8011e9e7e8540e0e64a3ff223`

正式标签：

`paper-exp-baseline-20260801`

先执行：

```bash
cd "/home/peizeyu2026/smart_wolf_project/ZTT-OR/20260715 ZTT-OR"
git fetch origin
git checkout master
git pull --ff-only origin master
git rev-parse HEAD
git rev-parse 'paper-exp-baseline-20260801^{}'
git status --short
```

标签实际提交必须为：

`0dd725354732f9b8011e9e7e8540e0e64a3ff223`

检查核心代码相对正式基准没有变化：

```bash
git diff --name-only \
  0dd725354732f9b8011e9e7e8540e0e64a3ff223..HEAD \
  -- "5_重构代码v2/src" "5_重构代码v2/include"
```

预期无输出；若有输出，停止并报告。

## 二、上传A1正式配置

将压缩包中的：

```text
5_重构代码v2/6_数值实验/
└── 01_实验矩阵/
    └── Part1_RQ4/
        └── A1_cplex_vs_bp/
            ├── FORMAL_EXPERIMENT_MATRIX_A1_20260801_1922.yaml
            └── FORMAL_EXPERIMENT_MATRIX_A1_20260801_1922.md
```

复制到仓库同一路径，不得覆盖已有同名文件。

校验YAML：

```bash
python - <<'PY'
from pathlib import Path
import yaml

p = Path(
    "5_重构代码v2/6_数值实验/01_实验矩阵/Part1_RQ4/"
    "A1_cplex_vs_bp/FORMAL_EXPERIMENT_MATRIX_A1_20260801_1922.yaml"
)
with p.open("r", encoding="utf-8") as f:
    data = yaml.safe_load(f)

assert data["run_counts"]["base_pairs"] == 90
assert data["run_counts"]["method_runs"] == 180
assert len(data["formal_random_seeds"]) == 10
print("A1 YAML PASS")
PY
```

提交配置：

```bash
git add "5_重构代码v2/6_数值实验/01_实验矩阵/Part1_RQ4/A1_cplex_vs_bp"
git diff --cached --stat
git diff --cached --name-status
git commit -m "Add Part 1 A1 formal experiment configuration"
git push origin master
```

记录配置提交SHA。

## 三、实验实现边界

优先复用现有：

- `test_bp_integration`
- 完整枚举主问题逻辑
- 固定`w` BP入口
- 原模型可行性验证函数

若需要新增A1实验编排脚本，只允许放到：

```text
5_重构代码v2/6_数值实验/05_运行脚本/Part1_RQ4/A1/
```

允许新增：

- 完整可行列枚举调用；
- 固定`w`批量编排；
- CPLEX与BP结果提取；
- 可行性复核；
- CSV汇总；
- MANIFEST与报告生成。

不得修改：

```text
5_重构代码v2/src/
5_重构代码v2/include/
```

不得重新实现与核心求解器不同的列利润或投资成本逻辑。

## 四、正式设计

规模：

```text
3×5
3×10
5×10
```

固定减排率：

```text
w_j=0.0
w_j=0.4
w_j=0.8
```

正式种子：

```text
101, 202, 303, 404, 505,
606, 707, 808, 909, 1010
```

总计90组配对、180次方法级运行。

A1不运行GA。固定`w`直接传入，不经过3位或10位编码。

## 五、共同正式参数

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

A1覆盖：

```yaml
run_mode: fixed_w_validation
ga_enabled: false
parallel_fitness: false
num_threads: 1
```

## 六、完整列CPLEX要求

参考模型必须：

1. 为每个DC枚举当前模型定义下的全部可行服务集合；
2. 使用与BP相同的列利润和投资成本逻辑；
3. 建立完整整数主问题；
4. CPLEX状态为`OPTIMAL`；
5. MIP gap为0；
6. 不得用RMP、LP松弛或缺列模型替代。

记录：

- 每个DC列数；
- 总完整列数；
- 列唯一性；
- CPLEX目标值；
- 状态、gap、墙钟时间；
- 参考解可行性。

## 七、BP要求

BP必须：

- 使用同一实例和固定`w`；
- 状态为`OPTIMAL_CERTIFIED`；
- 存在整数解；
- gap为0；
- 无剩余活动节点；
- 记录CG迭代、分支节点、处理与剪枝节点；
- 将最终解代回原固定`w`模型检查可行性。

## 八、单组正式前验收

先运行：

```text
num_dc=3
num_retailers=5
random_seed=101
uniform_w=0.4
```

目录：

```text
results/paper_experiments/part1_algorithm_validation/
A1_cplex_vs_bp/precheck_3x5_seed101_w040_YYYYMMDD_HHMMSS/
```

MANIFEST：

```text
experiment_type: A1 single-pair pre-formal acceptance
formal_replication: false
paper_statistics: excluded
```

必须生成：

```text
A1_SINGLE_PAIR_ACCEPTANCE.md
A1_SINGLE_PAIR_RESULT.csv
MANIFEST.txt
config_used.yaml
resolved_config.yaml
```

通过条件：

- CPLEX为`OPTIMAL`且gap=0；
- BP为`OPTIMAL_CERTIFIED`且gap=0；
- 目标差在正式容差内；
- 两套解均通过原模型可行性检查；
- 无完整列遗漏、重复列或成本口径差异。

若失败，停止，不得启动90组。

验收通过后，将结果提交到：

```text
5_重构代码v2/6_数值实验/02_验收记录/Part1_RQ4/
A1_single_pair_precheck_YYYYMMDD_HHMMSS/
```

提交信息：

```text
Add Part 1 A1 single-pair acceptance
```

## 九、90组正式配对

验收通过后，创建：

```text
results/paper_experiments/part1_algorithm_validation/
A1_cplex_vs_bp/formal_90pairs_YYYYMMDD_HHMMSS/
```

建议结构：

```text
formal_90pairs_YYYYMMDD_HHMMSS/
├── BATCH_MANIFEST.txt
├── A1_FORMAL_PAIRED_RESULTS.csv
├── A1_FORMAL_SUMMARY.csv
├── A1_FORMAL_RESULTS_REPORT.md
├── size_3x5/
├── size_3x10/
└── size_5x10/
```

运行顺序：

1. 3×5：w=0.0、0.4、0.8，各10个种子；
2. 3×10：w=0.0、0.4、0.8，各10个种子；
3. 5×10：w=0.0、0.4、0.8，各10个种子。

每组先运行完整列CPLEX，再运行BP。

## 十、比较规则

```text
absolute_difference =
abs(cplex_objective - bp_objective)

relative_difference =
abs(cplex_objective - bp_objective) / abs(cplex_objective)
```

决策向量不同不自动判定失败。若两种方法都认证最优、目标值匹配且两套解均可行，可标记：

```text
equivalent_multiple_optima: true
pair_valid: true
```

## 十一、异常停止规则

若任何正式配对出现以下问题，保留全部结果并停止剩余批次：

- 目标值实质不一致；
- CPLEX不是认证最优；
- BP不是`OPTIMAL_CERTIFIED`；
- 任一解无法通过原模型可行性检查；
- 完整列遗漏或重复影响模型；
- 投资成本、碳成本或列利润口径不一致；
- 配置解析结果与冻结配置不同。

不得：

- 换种子；
- 改固定`w`；
- 放宽容差；
- 修改时间限制；
- 修改BP参数；
- 覆盖失败记录继续运行。

## 十二、正式结果文件

生成：

```text
A1_FORMAL_PAIRED_RESULTS.csv
A1_FORMAL_SUMMARY.csv
A1_FORMAL_RESULTS_REPORT.md
BATCH_MANIFEST.txt
```

`A1_FORMAL_PAIRED_RESULTS.csv`每组至少包含：

```text
size_id
num_dc
num_retailers
random_seed
fixed_w
complete_column_count
cplex_status
cplex_objective
cplex_gap
cplex_wall_time_sec
bp_status
bp_objective
bp_gap
bp_wall_time_sec
absolute_objective_difference
relative_objective_difference
objective_match
reference_solution_feasible
bp_solution_feasible
decision_match
equivalent_multiple_optima
bp_cg_iterations
bp_branch_nodes
bp_processed_nodes
bp_pruned_nodes
bp_remaining_active_nodes
pair_valid
notes
```

`A1_FORMAL_SUMMARY.csv`按“规模×固定w”汇总：

- 配对数；
- CPLEX认证率；
- BP认证率；
- 目标一致数与一致率；
- 最大绝对差；
- 最大相对差；
- CPLEX时间均值/标准差/最小/最大；
- BP时间均值/标准差/最小/最大；
- 平均CG迭代；
- 平均分支节点；
- 等价多最优方案数；
- 异常数。

标准差使用`ddof=1`。

## 十三、正式报告

`A1_FORMAL_RESULTS_REPORT.md`包括：

1. 实验设计；
2. 完整列模型与列数核查；
3. 90组完成情况；
4. CPLEX与BP认证状态；
5. 最大绝对和相对目标差；
6. 等价最优方案；
7. 时间比较；
8. 异常情况；
9. 最终结论。

完整通过时写：

```text
PASS — A1 fixed-w Branch-and-Price validation completed
```

## 十四、GitHub正式结果归档

上传到：

```text
5_重构代码v2/6_数值实验/03_正式结果/Part1_RQ4/
A1_cplex_vs_bp/formal_90pairs_YYYYMMDD_HHMMSS/
```

至少上传：

```text
BATCH_MANIFEST.txt
A1_FORMAL_PAIRED_RESULTS.csv
A1_FORMAL_SUMMARY.csv
A1_FORMAL_RESULTS_REPORT.md
```

服务器保留完整原始日志和逐组目录；大型重复日志、编译产物和CPLEX临时文件不上传。

若新增A1编排脚本，一并上传：

```text
5_重构代码v2/6_数值实验/05_运行脚本/Part1_RQ4/A1/
```

## 十五、Git提交

只暂存A1相关目录：

```bash
git add \
  "5_重构代码v2/6_数值实验/03_正式结果/Part1_RQ4/A1_cplex_vs_bp" \
  "5_重构代码v2/6_数值实验/05_运行脚本/Part1_RQ4/A1"
```

没有新增脚本时，只暂存结果目录。

不要使用`git add -A`。

提交信息：

```text
Add Part 1 A1 formal validation results
```

然后：

```bash
git commit -m "Add Part 1 A1 formal validation results"
git push origin master
```

## 十六、完成后输出

请输出：

- A1配置提交SHA；
- 单组验收目录与结论；
- 单组验收Git提交SHA；
- 正式批次目录；
- 完成配对数/90；
- CPLEX认证数；
- BP认证数；
- 目标一致数；
- 最大绝对差；
- 最大相对差；
- 等价多最优方案数；
- CPLEX平均时间；
- BP平均时间；
- 异常数；
- 正式结果Git提交SHA；
- GitHub结果路径；
- 是否修改核心代码；
- 最终结论。

完成后停止，不要自动启动A3。
