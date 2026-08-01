# 03_正式结果

本目录用于存放Codex从服务器回传的正式实验结果。

建议结构：

```text
03_正式结果/
├── Part1_RQ4/
│   ├── A1_cplex_vs_bp/
│   ├── A2_grid_vs_ga_bp/
│   └── A3_scalability/
├── Part2_RQ1/
├── Part3_RQ2/
├── Part4_RQ3/
└── Part5_real_world_case/
```

每次运行目录至少保留：

- `config_used.yaml`
- `resolved_config.yaml`
- `run.log`
- `report.txt`
- `result.csv`
- `convergence.csv`
- `MANIFEST.txt`

大型原始日志、二进制文件或中间缓存不建议直接提交GitHub；可提交压缩摘要、CSV、Markdown报告和MANIFEST，并在README中记录服务器原始目录。
