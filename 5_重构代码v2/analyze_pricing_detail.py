#!/usr/bin/env python3
"""分析pricing子问题每轮的RC和列产出——到底在产出什么列？"""

import re
from collections import defaultdict

LOG_FILE = "diag_10x30.log"

def main():
    with open(LOG_FILE, "r") as f:
        lines = f.readlines()

    # 提取每轮CG的日志信息
    # 格式: "CG iter X: obj=..., maxRC=..., +N cols"
    cg_iters = []
    diag_iters = []
    diag_cols_info = []

    for line in lines:
        line = line.strip()
        # "CG iter X: obj=XXX, maxRC=XXX, +N cols"
        m = re.search(r"CG iter (\d+): obj=(-?\d+\.?\d*(?:e[+-]?\d+)?), maxRC=(-?\d+\.?\d*(?:e[+-]?\d+)?), \+(\d+) cols", line)
        if m:
            cg_iters.append({
                "iter": int(m.group(1)),
                "obj": float(m.group(2)),
                "maxRC": float(m.group(3)),
                "newCols": int(m.group(4)),
            })
            continue

        # "[DIAG] iter=X posCols=Y newCols=Z effImprove=W updated=U"
        m2 = re.search(r"\[DIAG\] iter=(\d+) posCols=(\d+) newCols=(\d+) effImprove=(\d+) updated=(\d+)", line)
        if m2:
            diag_cols_info.append({
                "iter": int(m2.group(1)),
                "posCols": int(m2.group(2)),
                "newCols": int(m2.group(3)),
                "effImprove": int(m2.group(4)),
                "effectiveImproveCols": int(m2.group(4)),
                "updated": int(m2.group(5)),
            })
            continue

    # 分离根节点和子节点数据
    # BB-DIAG标记了节点切换
    current_node = "root"
    node_cg_iters = defaultdict(list)
    node_diag_info = defaultdict(list)

    for line in lines:
        line = line.strip()
        if "[BB-DIAG]" in line:
            m = re.search(r"dc=(\d+),retailer=(\d+)", line)
            if m:
                # 简化节点名
                current_node = f"dc{m.group(1)}_r{m.group(2)}"
            continue
        # CG iter行
        m = re.search(r"CG iter (\d+): obj=(-?\d+\.?\d*(?:e[+-]?\d+)?), maxRC=(-?\d+\.?\d*(?:e[+-]?\d+)?), \+(\d+) cols", line)
        if m:
            node_cg_iters[current_node].append({
                "iter": int(m.group(1)),
                "obj": float(m.group(2)),
                "maxRC": float(m.group(3)),
                "newCols": int(m.group(4)),
            })
            continue
        m2 = re.search(r"\[DIAG\] iter=(\d+) posCols=(\d+) newCols=(\d+) effImprove=(\d+) updated=(\d+)", line)
        if m2:
            node_diag_info[current_node].append({
                "iter": int(m2.group(1)),
                "posCols": int(m2.group(2)),
                "newCols": int(m2.group(3)),
                "effImprove": int(m2.group(4)),
                "updated": int(m2.group(5)),
            })
            continue

    # ==================== 根节点分析 ====================
    print("=" * 80)
    print("【根节点】CG迭代详情")
    print("=" * 80)

    root_iters = node_cg_iters["root"]
    root_diag = node_diag_info["root"]

    print(f"\n总轮数: {len(root_iters)}")
    print(f"初始obj: {root_iters[0]['obj']:.2f}")
    print(f"最终obj: {root_iters[-1]['obj']:.2f}")
    print(f"obj改善: {root_iters[-1]['obj'] - root_iters[0]['obj']:.2f}")

    # 每轮列产出分析
    print("\n--- 每轮列产出 ---")
    total_posCols = sum(d["posCols"] for d in root_diag)
    total_newCols = sum(d["newCols"] for d in root_diag)
    total_effImprove = sum(d["effImprove"] for d in root_diag)
    total_updated = sum(d["updated"] for d in root_diag)

    print(f"总posCols(RC>0): {total_posCols}")
    print(f"总newCols(去重后新增): {total_newCols}")
    print(f"总effImprove(真能改进): {total_effImprove}")
    print(f"总updated(更新已有列系数): {total_updated}")

    # 有效改进率
    if total_posCols > 0:
        print(f"有效改进率: {total_effImprove/total_posCols:.2%}")
        print(f"重复列率: {(total_posCols - total_newCols - total_updated)/total_posCols:.2%}")

    # maxRC趋势
    print("\n--- maxRC趋势 ---")
    for i, it in enumerate(root_iters):
        if i < 10 or i > len(root_iters) - 10 or i % 10 == 0:
            diag_entry = None
            for d in root_diag:
                if d["iter"] == it["iter"]:
                    diag_entry = d
                    break
            extra = ""
            if diag_entry:
                extra = f" posCols={diag_entry['posCols']} new={diag_entry['newCols']} eff={diag_entry['effImprove']}"
            print(f"  iter {it['iter']:3d}: obj={it['obj']:12.2f} maxRC={it['maxRC']:8.4f} +{it['newCols']}cols{extra}")

    # ==================== 子节点分析 ====================
    child_nodes = sorted(set(k for k in node_cg_iters.keys() if k != "root"))[:4]
    for cn in child_nodes:
        cn_iters = node_cg_iters[cn]
        cn_diag = node_diag_info[cn]
        print(f"\n{'=' * 80}")
        print(f"【子节点 {cn}】CG迭代详情")
        print(f"{'=' * 80}")
        print(f"总轮数: {len(cn_iters)}")
        if cn_iters:
            print(f"初始obj: {cn_iters[0]['obj']:.2f}")
            print(f"最终obj: {cn_iters[-1]['obj']:.2f}")
            print(f"obj改善: {cn_iters[-1]['obj'] - cn_iters[0]['obj']:.2f}")

            total_pos = sum(d["posCols"] for d in cn_diag)
            total_new = sum(d["newCols"] for d in cn_diag)
            total_eff = sum(d["effImprove"] for d in cn_diag)
            total_upd = sum(d["updated"] for d in cn_diag)

            print(f"总posCols: {total_pos}  newCols: {total_new}  effImprove: {total_eff}  updated: {total_upd}")
            if total_pos > 0:
                print(f"有效改进率: {total_eff/total_pos:.2%}")
                print(f"重复列率: {(total_pos - total_new - total_upd)/total_pos:.2%}")

            # maxRC趋势（抽样）
            print("\n--- maxRC趋势（前10轮+后10轮+每50轮抽样）---")
            for i, it in enumerate(cn_iters):
                if i < 10 or i > len(cn_iters) - 10 or i % 50 == 0:
                    diag_entry = None
                    for d in cn_diag:
                        if d["iter"] == it["iter"]:
                            diag_entry = d
                            break
                    extra = ""
                    if diag_entry:
                        extra = f" posCols={diag_entry['posCols']} new={diag_entry['newCols']} eff={diag_entry['effImprove']}"
                    print(f"  iter {it['iter']:4d}: obj={it['obj']:12.2f} maxRC={it['maxRC']:8.4f} +{it['newCols']}cols{extra}")

    # ==================== 关键对比 ====================
    print(f"\n{'=' * 80}")
    print("【关键对比】根节点 vs 子节点")
    print(f"{'=' * 80}")
    print(f"{'节点':<20} {'轮数':>6} {'初始obj':>12} {'最终obj':>12} {'总newCols':>10} {'总effImprove':>14} {'effImprove率':>14}")
    print("-" * 80)

    root_new = sum(d["newCols"] for d in root_diag)
    root_eff = sum(d["effImprove"] for d in root_diag)
    root_pos = sum(d["posCols"] for d in root_diag)
    print(f"{'root':<20} {len(root_iters):>6} {root_iters[0]['obj']:>12.2f} {root_iters[-1]['obj']:>12.2f} {root_new:>10} {root_eff:>14} {root_eff/max(root_pos,1):>14.2%}")

    for cn in child_nodes:
        cn_iters2 = node_cg_iters[cn]
        cn_diag2 = node_diag_info[cn]
        cn_new = sum(d["newCols"] for d in cn_diag2)
        cn_eff = sum(d["effImprove"] for d in cn_diag2)
        cn_pos = sum(d["posCols"] for d in cn_diag2)
        if cn_iters2:
            print(f"{cn:<20} {len(cn_iters2):>6} {cn_iters2[0]['obj']:>12.2f} {cn_iters2[-1]['obj']:>12.2f} {cn_new:>10} {cn_eff:>14} {cn_eff/max(cn_pos,1):>14.2%}")

    # ==================== 重复列比例 ====================
    print(f"\n{'=' * 80}")
    print("【重复列比例】根节点每10轮抽样")
    print(f"{'=' * 80}")
    for d in root_diag:
        if d["iter"] % 10 == 0 or d["iter"] < 5:
            dup_rate = (d["posCols"] - d["newCols"] - d["updated"]) / max(d["posCols"], 1)
            print(f"  iter {d['iter']:3d}: posCols={d['posCols']} new={d['newCols']} eff={d['effImprove']} dup_rate={dup_rate:.2%}")

if __name__ == "__main__":
    main()