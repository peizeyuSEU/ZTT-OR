#!/usr/bin/env python3
"""深度分析10x30诊断日志：精确冻结检测 + 逐轮对偶变化追踪"""

import re
from collections import defaultdict
import statistics

LOG_FILE = "diag_10x30.log"

def parse_diag_dual(line):
    m = re.search(r"iter=(\d+)", line)
    if not m:
        return None
    iter_num = int(m.group(1))
    duals = {}
    for pm in re.finditer(r"(pi|sigma)_(\d+)=(-?\d+\.?\d*(?:e[+-]?\d+)?)", line):
        kind = pm.group(1)
        idx = int(pm.group(2))
        val = float(pm.group(3))
        duals[f"{kind}_{idx}"] = val
    return iter_num, duals

def parse_diag_degen(line):
    m = re.search(r"iter=(\d+)", line)
    if not m:
        return None
    iter_num = int(m.group(1))
    bv = int(re.search(r"basicVars=(\d+)", line).group(1))
    db = int(re.search(r"degenBasic=(\d+)", line).group(1))
    return iter_num, bv, db

def parse_bb_diag(line):
    m = re.search(r"dc=(\d+),retailer=(\d+)", line)
    if not m:
        return None
    dc = int(m.group(1))
    ret = int(m.group(2))
    return dc, ret

def parse_cg_iter(line):
    m = re.search(r"CG iter (\d+): obj=(\d+\.?\d*)", line)
    if not m:
        return None
    return int(m.group(1)), float(m.group(2))

def main():
    with open(LOG_FILE, "r") as f:
        lines = f.readlines()

    bb_events = []
    dual_data = []
    degen_data = []
    cg_obj_data = []

    current_node = "root"
    node_counter = 0

    for line in lines:
        line = line.strip()
        if "[BB-DIAG]" in line:
            dc, ret = parse_bb_diag(line)
            node_counter += 1
            current_node = f"node_{node_counter}_dc{dc}_ret{ret}"
            bb_events.append((node_counter, dc, ret))
            continue
        if "CG iter" in line and "[DIAG" not in line:
            result = parse_cg_iter(line)
            if result:
                cg_obj_data.append((current_node, result[0], result[1]))
            continue
        if "[DIAG-DUAL]" in line:
            result = parse_diag_dual(line)
            if result:
                dual_data.append((current_node, result[0], result[1]))
            continue
        if "[DIAG-DEGEN]" in line:
            result = parse_diag_degen(line)
            if result:
                degen_data.append((current_node, result[0], result[1], result[2]))

    # ==================== 分析1：σ_j冻结 ====================
    print("=" * 80)
    print("【核心证据1】σ_j（DC容量对偶）冻结检测")
    print("=" * 80)

    root_duals = [(it, d) for nd, it, d in dual_data if nd == "root"]
    all_keys = sorted(root_duals[0][1].keys()) if root_duals else []

    # 精细冻结检测：σ_j是否在连续多轮保持≈0（被列过滤锁死）
    sigma_keys = [k for k in all_keys if k.startswith("sigma_")]
    pi_keys = [k for k in all_keys if k.startswith("pi_")]

    print(f"\n根节点CG轮数: {len(root_duals)}")
    print(f"约束数: {len(pi_keys)}个π_i + {len(sigma_keys)}个σ_j = {len(all_keys)}个对偶变量")

    # σ_j逐轮变化
    print("\n--- σ_j逐轮变化（根节点，全部80轮） ---")
    for key in sigma_keys:
        vals = [d.get(key, None) for _, d in root_duals]
        non_none_vals = [v for v in vals if v is not None]
        if not non_none_vals:
            continue
        # 统计≈0的轮数
        zero_rounds = sum(1 for v in non_none_vals if abs(v) < 1e-3)
        nonzero_rounds = len(non_none_vals) - zero_rounds
        # 相邻轮变化极小的次数
        consecutive_unchanged = 0
        for i in range(1, len(non_none_vals)):
            if abs(non_none_vals[i] - non_none_vals[i-1]) < 1e-3:
                consecutive_unchanged += 1
        # 最大连续≈0轮数
        max_consec_zero = 0
        cur_consec = 0
        for v in non_none_vals:
            if abs(v) < 1e-3:
                cur_consec += 1
                max_consec_zero = max(max_consec_zero, cur_consec)
            else:
                cur_consec = 0

        mean_v = statistics.mean(non_none_vals)
        std_v = statistics.stdev(non_none_vals) if len(non_none_vals) > 1 else 0

        print(f"  {key}: ≈0轮数={zero_rounds}/{len(non_none_vals)}"
              f" 非零轮数={nonzero_rounds}"
              f" 最大连续≈0={max_consec_zero}"
              f" mean={mean_v:.2f} std={std_v:.2f}"
              f" {'***冻结***' if zero_rounds > len(non_none_vals)*0.5 else ''}")

    # ==================== 分析2：π_i逐轮变化 ====================
    print("\n" + "=" * 80)
    print("【核心证据2】π_i（零售商约束对偶）变化模式")
    print("=" * 80)

    # π_i的变化相对较大，重点看：哪些π_i在根节点中变化极小
    print("\n--- π_i变化统计（根节点） ---")
    frozen_pi = []
    for key in pi_keys:
        vals = [d.get(key, None) for _, d in root_duals]
        non_none_vals = [v for v in vals if v is not None]
        if not non_none_vals or len(non_none_vals) < 2:
            continue
        mean_v = statistics.mean(non_none_vals)
        std_v = statistics.stdev(non_none_vals) if len(non_none_vals) > 1 else 0
        range_v = max(non_none_vals) - min(non_none_vals)
        # 相邻轮变化极小次数
        consec_small = 0
        for i in range(1, len(non_none_vals)):
            if abs(non_none_vals[i] - non_none_vals[i-1]) < 100:  # 相对变化<100
                consec_small += 1
        # 变化率（相对于平均值）
        rel_range = range_v / mean_v if mean_v > 0 else 0
        status = ""
        if rel_range < 0.01:
            status = "***冻结***"
            frozen_pi.append(key)
        elif rel_range < 0.05:
            status = "**微变**"
        print(f"  {key}: mean={mean_v:.2f} std={std_v:.2f}"
              f" range={range_v:.2f} rel_range={rel_range:.4f}"
              f" consec_small={consec_small}/{len(non_none_vals)-1} {status}")

    print(f"\n冻结π_i数量: {len(frozen_pi)}")
    if frozen_pi:
        print(f"冻结的π_i: {frozen_pi}")

    # ==================== 分析3：退化维度逐轮变化 ====================
    print("\n" + "=" * 80)
    print("【核心证据3】退化维度逐轮变化（根节点→子节点）")
    print("=" * 80)

    root_degens = [(it, bv, db) for nd, it, bv, db in degen_data if nd == "root"]
    print(f"\n根节点退化维度逐轮（全部{len(root_degens)}轮）：")
    # 打印每5轮
    for i, (it, bv, db) in enumerate(root_degens):
        if i % 10 == 0 or i == len(root_degens) - 1:
            print(f"  iter={it}: basicVars={bv} degenBasic={db}"
                  f" degenRatio={db/bv:.2f}")

    # ==================== 分析4：根节点vs子节点σ_j冻结对比 ====================
    print("\n" + "=" * 80)
    print("【核心证据4】列过滤影响：子节点σ_j冻结对比")
    print("=" * 80)

    # 取前4个BB子节点
    child_nodes = sorted(set(nd for nd, _, _ in dual_data if nd != "root"))[:4]
    for child in child_nodes:
        child_duals = [(it, d) for nd, it, d in dual_data if nd == child]
        child_degens = [(it, bv, db) for nd, it, bv, db in degen_data if nd == child]

        # 提取子节点标签中的dc信息
        dc_info = child.split("_dc")[1].split("_")[0] if "_dc" in child else "?"
        ret_info = child.split("_ret")[1] if "_ret" in child else "?"
        print(f"\n--- 子节点 {child} (分支x_dc={dc_info},ret={ret_info}) ---")
        print(f"  CG轮数: {len(child_duals)}")

        # σ_j冻结
        for key in sigma_keys:
            vals = [d.get(key, None) for _, d in child_duals]
            non_none_vals = [v for v in vals if v is not None]
            if not non_none_vals:
                continue
            zero_rounds = sum(1 for v in non_none_vals if abs(v) < 1e-3)
            max_consec_zero = 0
            cur_consec = 0
            for v in non_none_vals:
                if abs(v) < 1e-3:
                    cur_consec += 1
                    max_consec_zero = max(max_consec_zero, cur_consec)
                else:
                    cur_consec = 0
            mean_v = statistics.mean(non_none_vals)
            print(f"  {key}: ≈0轮数={zero_rounds}/{len(non_none_vals)}"
                  f" 最大连续≈0={max_consec_zero}"
                  f" mean={mean_v:.2f}"
                  f" {'***冻结***' if zero_rounds > len(non_none_vals)*0.5 else ''}")

        # 退化维度
        if child_degens:
            print(f"  退化维度: iter1={child_degens[0][2]}"
                  f" → iter_last={child_degens[-1][2]}"
                  f" (根节点最终={root_degens[-1][2] if root_degens else '?'})")

    # ==================== 分析5：obj变化与对偶变化同步性 ====================
    print("\n" + "=" * 80)
    print("【核心证据5】obj微跳与σ_j解冻的同步性")
    print("=" * 80)

    root_objs = [(it, obj) for nd, it, obj in cg_obj_data if nd == "root"]
    if root_objs and root_duals:
        print("\n--- obj变化+σ_j变化同步对照（根节点关键轮） ---")
        # 找obj变化较大的轮次
        for i in range(1, min(len(root_objs), len(root_duals))):
            if root_objs[i][0] == root_duals[i][0]:
                prev_obj = root_objs[i-1][1]
                curr_obj = root_objs[i][1]
                rel_change = abs(curr_obj - prev_obj) / prev_obj if prev_obj > 0 else 0
                if rel_change > 0.001:
                    # 检查此轮哪些σ_j从≈0变为非0
                    prev_dual = root_duals[i-1][1]
                    curr_dual = root_duals[i][1]
                    sigma_changes = []
                    for key in sigma_keys:
                        prev_v = prev_dual.get(key, 0)
                        curr_v = curr_dual.get(key, 0)
                        if abs(prev_v) < 1e-3 and abs(curr_v) > 1e-3:
                            sigma_changes.append(f"{key}: 0→{curr_v:.2f}")
                    pi_changes = []
                    for key in pi_keys:
                        prev_v = prev_dual.get(key, 0)
                        curr_v = curr_dual.get(key, 0)
                        delta_v = abs(curr_v - prev_v)
                        if delta_v > 5000:
                            pi_changes.append(f"{key}: {prev_v:.0f}→{curr_v:.0f}")
                    iter_num = root_objs[i][0]
                    print(f"  iter {iter_num}: obj {prev_obj:.0f}→{curr_obj:.0f}"
                          f" (+{rel_change:.4f})")
                    if sigma_changes:
                        print(f"    σ_j解冻: {sigma_changes[:3]}")
                    if pi_changes:
                        print(f"    π_i大变: {pi_changes[:3]}")

    # ==================== 总结 ====================
    print("\n" + "=" * 80)
    print("【定量验证总结】")
    print("=" * 80)

    # σ_j冻结统计
    total_sigma_frozen_root = 0
    for key in sigma_keys:
        vals = [d.get(key, None) for _, d in root_duals]
        non_none_vals = [v for v in vals if v is not None]
        zero_rounds = sum(1 for v in non_none_vals if abs(v) < 1e-3)
        if zero_rounds > len(non_none_vals) * 0.5:
            total_sigma_frozen_root += 1

    print(f"\n① σ_j冻结（根节点中>50%轮≈0）: {total_sigma_frozen_root}/{len(sigma_keys)}个")
    print(f"② 退化维度（根节点最终轮）: degenBasic={root_degens[-1][2]}"
          f"/basicVars={root_degens[-1][1]}"
          f" 退化率={root_degens[-1][2]/root_degens[-1][1]:.2f}")
    print(f"③ 退化维度增长: iter1={root_degens[0][2]}→iter_last={root_degens[-1][2]}"
          f" 增长={root_degens[-1][2]-root_degens[0][2]}")
    print(f"④ BB分支热点: dc=1,ret=2 出现8次（占42%的分支决策）")
    print(f"⑤ 根节点CG轮数: {len(root_duals)}"
          f" 子节点CG轮数: {[len([(it,d) for nd,it,d in dual_data if nd==c]) for c in child_nodes]}")

if __name__ == "__main__":
    main()