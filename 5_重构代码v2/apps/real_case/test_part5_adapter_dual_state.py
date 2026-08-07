#!/usr/bin/env python3
import json
import shutil
import subprocess
import tempfile
from pathlib import Path

DATA_REPO = Path("/home/peizeyu2026/smart_wolf_project/datasets/YRD-Steel-v02")
ADAPTER = Path("/home/peizeyu2026/smart_wolf_project/part5_integration_worktree/5_重构代码v2/apps/real_case/part5_adapter.py")
BASE = "5b22a235b6c6712de0d98441fc4a4d5e6780e3c5"
HIST = "b9fc758dbe11762882bf66cbbc7672da0918adb7"

def run(data_dir, output_dir):
    p = subprocess.run(
        ["python3", str(ADAPTER), "--data-dir", str(data_dir),
         "--intracity-distance-km", "10", "--output-dir", str(output_dir),
         "--validate-only"],
        text=True, capture_output=True,
    )
    payload = json.loads(p.stdout)
    return p.returncode, payload

def main():
    with tempfile.TemporaryDirectory(prefix="part5_dual_state_") as td:
        root = Path(td)
        hist_dir = root / "historical"
        pub_dir = root / "published"
        subprocess.run(["git", "-C", str(DATA_REPO), "worktree", "add", "--detach", str(hist_dir), HIST], check=True, stdout=subprocess.DEVNULL)
        subprocess.run(["git", "-C", str(DATA_REPO), "worktree", "add", "--detach", str(pub_dir), BASE], check=True, stdout=subprocess.DEVNULL)
        try:
            rc, out = run(hist_dir / "data/processed/v0.2.0-candidate", root / "out_hist")
            assert rc == 0 and out["status"] == "PASS" and out["dataset_state"] == "HISTORICAL_EXECUTION"
            rc, out = run(pub_dir / "data/processed/v0.2.0-candidate", root / "out_pub")
            assert rc == 0 and out["status"] == "PASS" and out["dataset_state"] == "CURRENT_PUBLISHED"

            cfg = pub_dir / "data/processed/v0.2.0-candidate/case_config.json"
            original_cfg = cfg.read_text()
            cfg.write_text(original_cfg.replace("14442.842886043889", "1.0"))
            rc, out = run(pub_dir / "data/processed/v0.2.0-candidate", root / "out_wrong_quota")
            assert rc != 0 and out["status"] == "FAIL"
            cfg.write_text(original_cfg)

            nodes = pub_dir / "data/processed/v0.2.0-candidate/nodes.csv"
            original_nodes = nodes.read_bytes()
            nodes.write_bytes(original_nodes + b"\n")
            rc, out = run(pub_dir / "data/processed/v0.2.0-candidate", root / "out_changed_input")
            assert rc != 0 and out["status"] == "FAIL"
            nodes.write_bytes(original_nodes)

            cfg.write_text(original_cfg.replace('0.39', '0.390001'))
            rc, out = run(pub_dir / "data/processed/v0.2.0-candidate", root / "out_changed_config")
            assert rc != 0 and out["status"] == "FAIL"
            cfg.write_text(original_cfg)
        finally:
            subprocess.run(["git", "-C", str(DATA_REPO), "worktree", "remove", "--force", str(hist_dir)], check=True)
            subprocess.run(["git", "-C", str(DATA_REPO), "worktree", "remove", "--force", str(pub_dir)], check=True)
    print("PASS: historical state")
    print("PASS: published state")
    print("PASS: wrong quota rejected")
    print("PASS: changed optimization input rejected")
    print("PASS: changed optimization config rejected")

if __name__ == "__main__":
    main()
