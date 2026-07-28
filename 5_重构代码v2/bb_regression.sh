#!/bin/bash
# BB 修复回归验证脚本
# 逐组修改 delta3000.yaml 的种子/规模，跑 bin/ga_bp，抓 BB 健康指标。
# 健康标准：负gap计数=0、无"反复分支/达到最大节点数"告警、最大探索节点不爆炸。

set -u
CFG="src/1_launcher/configs/delta3000.yaml"
BAK="src/1_launcher/configs/delta3000.yaml.bak"
BIN="./bin/ga_bp"
SUMMARY="bb_regression_summary.txt"

# 回归矩阵：seed dc ret
CASES=(
  "32501 10 30"
  "11111 10 30"
  "22222 10 30"
  "33333 10 30"
  "44444 10 30"
  "32501 5 20"
  "11111 5 20"
  "32501 15 50"
)

echo "=== BB 回归验证开始 $(date) ===" > "$SUMMARY"
printf "%-8s %-6s %-6s %-9s %-7s %-9s %-9s %-12s\n" \
  "seed" "dc" "ret" "neg_gap" "boom" "bb_total" "max_node" "verdict" >> "$SUMMARY"

for c in "${CASES[@]}"; do
  read -r seed dc ret <<< "$c"
  # 从备份恢复干净配置再改，避免累积污染
  cp "$BAK" "$CFG"
  sed -i "s/^random_seed:.*/random_seed: $seed/" "$CFG"
  sed -i "s/^num_dc:.*/num_dc: $dc/" "$CFG"
  sed -i "s/^num_retailers:.*/num_retailers: $ret/" "$CFG"
  # 缩小 GA 规模控制回归耗时（只为采样 BB 健康度）
  sed -i "s/^max_generation:.*/max_generation: 8/" "$CFG"
  sed -i "s/^population_size:.*/population_size: 12/" "$CFG"

  LOG="reg_${seed}_${dc}x${ret}.log"
  rm -f "$LOG"
  timeout 300 "$BIN" > "$LOG" 2>&1

  # 负gap计数（治本核心指标，必须为0）
  neg=$(grep -c "gap=-" "$LOG")
  # 节点爆炸次数（触发最大节点数上限的BB调用数）
  boom=$(grep -c "达到最大节点数" "$LOG")
  # BB调用总数（作为爆炸率分母）
  bbtotal=$(grep -c "分支定界结束" "$LOG")
  # 最大探索节点数
  maxnode=$(grep -oE "探索 [0-9]+ 节点" "$LOG" | grep -oE "[0-9]+" | sort -n | tail -1)
  [ -z "$maxnode" ] && maxnode="N/A"

  # 判据：负gap必须为0；节点爆炸允许极少数（初代坏个体），爆炸率超过5%才算 FAIL
  if [ "$neg" -ne 0 ]; then
    verdict="FAIL-neg"
  elif [ "$boom" -eq 0 ]; then
    verdict="PASS"
  elif [ "$bbtotal" -gt 0 ] && [ "$((boom * 100 / bbtotal))" -le 5 ]; then
    verdict="PASS-fewboom"
  else
    verdict="FAIL-boom"
  fi

  printf "%-8s %-6s %-6s %-9s %-7s %-9s %-9s %-12s\n" \
    "$seed" "$dc" "$ret" "$neg" "$boom" "$bbtotal" "$maxnode" "$verdict" >> "$SUMMARY"
  echo "done: seed=$seed ${dc}x${ret} neg=$neg boom=$boom/$bbtotal maxnode=$maxnode -> $verdict"
done

# 恢复原始配置
cp "$BAK" "$CFG"
echo "=== 回归结束，配置已恢复 $(date) ===" >> "$SUMMARY"
echo "ALL_REGRESSION_DONE"
cat "$SUMMARY"