/**
 * 单次运行启动器
 *
 * 用法：直接运行 ./bin/ga_bp
 * 切换配置：修改下面的 CONFIG_PATH 绝对路径，重新编译
 */

#include "../2_orchestrator/Orchestrator.h"

// ===== 在这里改路径切换配置 =====
static const std::string CONFIG_PATH =
    "/home/pei_zeyu/projects/cplex_code/5_\xe9\x87\x8d\xe6\x9e\x84\xe4\xbb\xa3\xe7\xa0\x81v2/src/1_launcher/configs/delta3000.yaml";
// ================================

int main() {
    Orchestrator orchestrator;
    orchestrator.initialize(CONFIG_PATH);
    orchestrator.run();
    orchestrator.finalize();
    return 0;
}
