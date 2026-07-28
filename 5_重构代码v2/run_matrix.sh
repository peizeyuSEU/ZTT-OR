#!/bin/sh
# 逐组串行跑16组矩阵,每组抓关键统计到汇总文件
# 不用 timeout 截断,跑到彻底完成(BB时间上限已放宽到7200s)

SUMMARY="/tmp/matrix_summary.txt"
: > "$SUMMARY"

echo "规模 w 分支节点数 剪枝节点数 CG迭代次数 总列数 总利润 总耗时" >> "$SUMMARY"

for DC_R in "20x70" "30x100" "40x150" "50x200"; do
    for WTAG in "00" "02" "05" "08"; do
        NAME="matrix_${DC_R}_w${WTAG}"
  LOG="/tmp/${NAME}.log"
        echo "===== 开始 ${NAME} $(date +%H:%M:%S) ====="
        ./bin/batch "$NAME" > "$LOG" 2>&1
        # 抓关键指标
        BB=$(grep "总分支节点数:" "$LOG" | tail -1 | grep -oE "[0-9]+")
        PR=$(grep "剪枝节点数:" "$LOG" | tail -1 | grep -oE "[0-9]+")
        CG=$(grep "列生成迭代次数:" "$LOG" | tail -1 | grep -oE "[0-9]+")
        COL=$(grep "总列数:" "$LOG" | tail -1 | grep -oE "[0-9]+")
        PROFIT=$(grep -E "总利润" "$LOG" | tail -1 | grep -oE "[0-9]+\.[0-9]+" | head -1)
        TIME=$(grep "总耗时:" "$LOG" | tail -1 | grep -oE "[0-9]+\.[0-9]+")
        echo "${DC_R} 0.${WTAG} ${BB} ${PR} ${CG} ${COL} ${PROFIT} ${TIME}s" >> "$SUMMARY"
        echo "  完成 ${NAME}: BB=${BB} CG=${CG} 利润=${PROFIT} 耗时=${TIME}s"
    done
done

echo "===== 全部完成 $(date +%H:%M:%S) ====="
echo "=== 汇总 ==="
cat "$SUMMARY"