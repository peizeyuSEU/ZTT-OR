#!/bin/sh
# 只跑8个大规模组(40x150/50x200 各4个w),600s时间上限统一预算
# 小规模8组已有真实数据,追加到同一汇总

SUMMARY="/tmp/matrix_summary.txt"

for DC_R in "40x150" "50x200"; do
    for WTAG in "00" "02" "05" "08"; do
        NAME="matrix_${DC_R}_w${WTAG}"
        LOG="/tmp/${NAME}.log"
        echo "===== 开始 ${NAME} $(date +%H:%M:%S) ====="
        ./bin/batch "$NAME" > "$LOG" 2>&1
        BB=$(grep "总分支节点数:" "$LOG" | tail -1 | grep -oE "[0-9]+")
        PR=$(grep "剪枝节点数:" "$LOG" | tail -1 | grep -oE "[0-9]+")
        CG=$(grep "列生成迭代次数:" "$LOG" | tail -1 | grep -oE "[0-9]+")
        COL=$(grep "总列数:" "$LOG" | tail -1 | grep -oE "[0-9]+")
        PROFIT=$(grep -E "总利润" "$LOG" | tail -1 | grep -oE "[0-9]+\.[0-9]+" | head -1)
        TIME=$(grep "总耗时:" "$LOG" | tail -1 | grep -oE "[0-9]+\.[0-9]+")
        HITLIMIT=$(grep -c "达到时间上限" "$LOG")
        echo "${DC_R} 0.${WTAG} ${BB} ${PR} ${CG} ${COL} ${PROFIT} ${TIME}s 撞600s上限=${HITLIMIT}" >> "$SUMMARY"
        echo "  完成 ${NAME}: BB=${BB} CG=${CG} 利润=${PROFIT} 耗时=${TIME}s 撞上限=${HITLIMIT}"
    done
done

echo "===== 大规模全部完成 $(date +%H:%M:%S) ====="
echo "=== 完整汇总 ==="
cat "$SUMMARY"