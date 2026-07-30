/**
 * 单次运行启动器
 *
 * 用法：./bin/ga_bp [配置文件路径]
 * 不传参数时使用下面的默认配置。
 */

#include "../2_orchestrator/Orchestrator.h"

// ===== 在这里改路径切换配置 =====
static const std::string CONFIG_PATH =
    "/home/peizeyu2026/smart_wolf_project/ZTT-OR/20260715 ZTT-OR/5_重构代码v2/src/1_launcher/configs/diag_10x30_multicol3.yaml";
// ================================

int main(int argc, char** argv) {
    const std::string configPath =
        argc > 1 ? std::string(argv[1]) : CONFIG_PATH;
    Orchestrator orchestrator;
    orchestrator.initialize(configPath);
    orchestrator.run();
    orchestrator.finalize();
    return 0;
}
