# Codex GitHub同步规范

## 一、拉取方式

在服务器项目根目录执行：

```bash
git fetch origin
git checkout master
git pull --ff-only origin master
```

目标目录：

```text
5_重构代码v2/6_数值实验/
```

## 二、正式运行前

1. 读取：
   - `00_全局基准/FORMAL_BASELINE_FINAL_20260801_1457.yaml`
   - `00_全局基准/EXPERIMENT_PROTOCOL_FINAL_20260801_1457.md`
   - 对应Part的实验矩阵YAML和MD。
2. 每次运行只覆盖矩阵明确允许的实验专属字段。
3. 生成并保存`config_used.yaml`。
4. 启动后保存`resolved_config.yaml`，核对实际解析结果。
5. 若解析结果与预期不一致，停止该批次，不得继续正式运行。

## 三、结果回传

正式结果应提交到：

```text
5_重构代码v2/6_数值实验/03_正式结果/
```

建议优先提交：

- 结果CSV；
- 汇总CSV；
- 运行MANIFEST；
- Markdown决策报告；
- 关键配置；
- 失败/超时清单；
- 小型收敛文件。

不要默认提交：

- CPLEX临时文件；
- 可执行文件；
- 大型缓存；
- 重复日志；
- 超大原始中间文件。

如单个文件较大，先生成摘要并在MANIFEST中记录服务器绝对路径。

## 四、提交规则

每完成一个独立批次再提交，例如：

```bash
git add "5_重构代码v2/6_数值实验"
git commit -m "Add Part 1 A2 grid precheck results"
git push origin master
```

不得把未完成、未经检查的正式结果和其他代码修改混在同一个提交中。

## 五、禁止事项

- 不覆盖冻结的全局基准；
- 不覆盖历史实验矩阵；
- 不删除失败、超时或较慢结果；
- 不用新种子替换失败种子；
- 不把预检结果混入正式统计；
- 不因单个实例运行慢而临时修改全局参数；
- 不在没有新版本说明的情况下混用不同代码、协议或矩阵。
