/**
 * 单次运行启动器
 *
 * 用法：直接运行 ./bin/ga_bp
 * 切换配置：修改下面的 CONFIG_PATH 绝对路径，重新编译
 */

#include "../2_orchestrator/Orchestrator.h"

// ===== 在这里改路径切换配置 =====
static const std::string CONFIG_PATH =
    "/home/peizeyu2026/smart_wolf_project/ZTT-OR/20260715 ZTT-OR/5_重构代码v2/src/1_launcher/configs/diag_10x30_multicol3.yaml";
// ================================

int main() {
    Orchestrator orchestrator;
    orchestrator.initialize(CONFIG_PATH);
    orchestrator.run();
    orchestrator.finalize();
    return 0;
}
