#!/usr/bin/env python3
"""分析10x30诊断日志：提取对偶变量冻结、退化维度、obj微跳的定量证据"""

import re
import sys
from collections import defaultdict

LOG_FILE = "diag_10x30.log"

def parse_diag_dual(line):
    """解析DIAG-DUAL行，返回(iter, {pi_i: val, sigma_j: val})"""
    m = re.search(r"iter=(\d+)", line)
    if not m:
        return None
    iter_num = int(m.group(1))
    duals = {}
    for pm in re.finditer(r"(pi|sigma)_(\d+)=(-?\d+\.?\d*)", line):
        kind = pm.group(1)
        idx = int(pm.group(2))
        val = float(pm.group(3))
        duals[f"{kind}_{idx}"] = val
    return iter_num, duals

def parse_diag_degen(line):
    """解析DIAG-DEGEN行，返回(iter, basicVars, degenBasic, degenDim)"""
    m = re.search(r"iter=(\d+)", line)
    if not m:
        return None
    iter_num = int(m.group(1))
    bv = int(re.search(r"basicVars=(\d+)", line).group(1))
    db = int(re.search(r"degenBasic=(\d+)", line).group(1))
    dd = int(re.search(r"degenDim=(\d+)", line).group(1))
    return iter_num, bv, db, dd

def parse_bb_diag(line):
    """解析BB-DIAG行，返回(dc, retailer, node_count)"""
    m = re.search(r"dc=(\d+),retailer=(\d+)", line)
    if not m:
        return None
    dc = int(m.group(1))
    ret = int(m.group(2))
    nc = int(re.search(r"节点数=(\d+)", line).group(1))
    return dc, ret, nc

def parse_cg_iter(line):
    """解析CG iter行，返回(iter, obj)"""
    m = re.search(r"CG iter (\d+): obj=(\d+\.?\d*)", line)
    if not m:
        return None
    return int(m.group(1)), float(m.group(2))

def main():
    with open(LOG_FILE, "r") as f:
        lines = f.readlines()

    # 区分不同BB节点的CG序列
    # 用"分支定界"或BB相关日志来切分节点
    bb_events = []
    dual_data = []  # [(node_label, iter, {pi: val, sigma: val})]
    degen_data = []  # [(node_label, iter, basicVars, degenBasic)]
    cg_obj_data = []  # [(node_label, iter, obj)]

    current_node = "root"
    node_counter = 0

    for line in lines:
        line = line.strip()

        # BB-DIAG → 新节点开始
        if "[BB-DIAG]" in line:
            dc, ret, nc = parse_bb_diag(line)
            node_counter += 1
            current_node = f"node_{node_counter}_dc{dc}_ret{ret}"
            bb_events.append((node_counter, dc, ret, nc))
            continue

        # CG iter (obj)
        if "CG iter" in line and "[DIAG" not in line:
            result = parse_cg_iter(line)
            if result:
                cg_obj_data.append((current_node, result[0], result[1]))
            continue

        # DIAG-DUAL
        if "[DIAG-DUAL]" in line:
            result = parse_diag_dual(line)
            if result:
                dual_data.append((current_node, result[0], result[1]))
            continue

        # DIAG-DEGEN
        if "[DIAG-DEGEN]" in line:
            result = parse_diag_degen(line)
            if result:
                degen_data.append((current_node, result[0], result[1], result[2]))

    print("=" * 80)
    print("1. BB分支事件汇总")
    print("=" * 80)
    # 统计分支变量的频率
    branch_freq = defaultdict(int)
    for _, dc, ret, nc in bb_events:
        branch_freq[f"dc={dc},ret={ret}"] += 1
    for var, cnt in sorted(branch_freq.items(), key=lambda x: -x[1]):
        print(f"  {var}: {cnt}次")

    print(f"\n  总分支节点数: {len(bb_events)}")
    print(f"  总CG迭代数: {len(dual_data)}")

    print("\n" + "=" * 80)
    print("2. 根节点CG：对偶变量逐轮变化（前5轮+后5轮）")
    print("=" * 80)

    root_duals = [(it, d) for nd, it, d in dual_data if nd == "root"]
    root_degens = [(it, bv, db) for nd, it, bv, db in degen_data if nd == "root"]

    if root_duals:
        print(f"  根节点CG轮数: {len(root_duals)}")
        # 找出哪些对偶变量被冻结（标准差极小）
        all_pi_keys = sorted([k for k in root_duals[0][1].keys() if k.startswith("pi_")])
        all_sigma_keys = sorted([k for k in root_duals[0][1].keys() if k.startswith("sigma_")])

        print("\n  --- 对偶变量冻结分析（根节点） ---")
        frozen_threshold = 1e-3  # 变化极小视为冻结

        for key in all_pi_keys + all_sigma_keys:
            vals = [d[key] for _, d in root_duals if key in d]
            if len(vals) < 2:
                continue
            mean_val = sum(vals) / len(vals)
            # 计算变化幅度：最大值-最小值
            min_val = min(vals)
            max_val = max(vals)
            range_val = max_val - min_val
            # 计算标准差
            std_val = (sum((v - mean_val)**2 for v in vals) / len(vals))**0.5
            # 计算变化率：有多少轮值没变
            unchanged = sum(1 for i in range(1, len(vals)) if abs(vals[i] - vals[i-1]) < 1e-3)
            status = "冻结" if range_val < 1e-3 else ("微变" if range_val < 1000 else "活跃")
            if status in ["冻结", "微变"]:
                print(f"  {key}: mean={mean_val:.2f} range={range_val:.2f} std={std_val:.2f}"
                      f" unchanged={unchanged}/{len(vals)-1} → {status}")

        print("\n  --- 退化维度逐轮变化（根节点） ---")
        for it, bv, db in root_degens[:5]:
            print(f"  iter={it}: basicVars={bv} degenBasic={db} degenDim={db}")
        if len(root_degens) > 5:
            print(f"  ... (共{len(root_degens)}轮)")
            for it, bv, db in root_degens[-5:]:
                print(f"  iter={it}: basicVars={bv} degenBasic={db} degenDim={db}")

    print("\n" + "=" * 80)
    print("3. 子节点vs根节点：对偶变量对比")
    print("=" * 80)

    # 取前3个子节点
    child_nodes = sorted(set(nd for nd, _, _ in dual_data if nd != "root"))[:3]
    for child in child_nodes:
        child_duals = [(it, d) for nd, it, d in dual_data if nd == child]
        child_degens = [(it, bv, db) for nd, it, bv, db in degen_data if nd == child]
        print(f"\n  --- {child} ---")
        print(f"  CG轮数: {len(child_duals)}")
        if child_duals:
            # 对比根节点最终轮的对偶变量
            root_last = root_duals[-1][1] if root_duals else {}
            child_first = child_duals[0][1] if child_duals else {}
            child_last = child_duals[-1][1] if child_duals else {}

            # 列过滤：子节点中哪些sigma_j或pi_i消失了
            print(f"  根节点最终轮有 {len(root_last)} 个对偶变量")
            print(f"  子节点第1轮有 {len(child_first)} 个对偶变量")

            # 找冻结的对偶变量
            for key in all_pi_keys + all_sigma_keys:
                vals = [d[key] for _, d in child_duals if key in d]
                if len(vals) < 2:
                    continue
                min_v = min(vals)
                max_v = max(vals)
                range_v = max_v - min_v
                status = "冻结" if range_v < 1e-3 else ("微变" if range_v < 1000 else "活跃")
                if status in ["冻结", "微变"]:
                    print(f"  {key}: range={range_v:.2f} → {status}")

            # 退化维度
            if child_degens:
                print(f"  退化维度: {child_degens[0][2]} → {child_degens[-1][2]}")

    print("\n" + "=" * 80)
    print("4. 根节点CG：obj逐轮变化（检测微跳）")
    print("=" * 80)

    root_objs = [(it, obj) for nd, it, obj in cg_obj_data if nd == "root"]
    if root_objs:
        print(f"  根节点CG轮数: {len(root_objs)}")
        for it, obj in root_objs[:5]:
            print(f"  iter={it}: obj={obj:.6f}")
        if len(root_objs) > 10:
            print(f"  ...")
            for it, obj in root_objs[-5:]:
                print(f"  iter={it}: obj={obj:.6f}")

        # 检测obj微跳：相邻轮obj变化>阈值但<1%
        print("\n  --- obj微跳检测 ---")
        for i in range(1, len(root_objs)):
            prev_obj = root_objs[i-1][1]
            curr_obj = root_objs[i][1]
            if prev_obj > 0:
                rel_change = abs(curr_obj - prev_obj) / prev_obj
                if rel_change > 0.0001 and rel_change < 0.01:
                    print(f"  iter {root_objs[i-1][0]}→{root_objs[i][0]}:"
                          f" obj {prev_obj:.6f}→{curr_obj:.6f}"
                          f" 相对变化={rel_change:.6f}")

if __name__ == "__main__":
    main()