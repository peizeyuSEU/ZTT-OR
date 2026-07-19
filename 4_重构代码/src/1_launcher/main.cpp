/**
 * 第1层：启动器 — 单次运行
 *
 * 读 config.yaml → 调 ExperimentRunner 运行 → 输出结果
 */

#include "../2_config/Config.h"
#include "../0_base/03_utils/Logger.h"
#include "ExperimentRunner.h"
#include <iostream>

int main(int argc, char** argv) {
    Config config;
    std::string configFile = "config.yaml";
    if (argc >= 2) configFile = argv[1];
    config.loadFromFile(configFile);
    config.print();

    mkdir(config.output_dir.c_str(), 0755);
    config.saveToFile(config.output_dir + "/config_used.yaml");

    Logger logger;
    logger.init(config.log_file, config.verbose);

    ExperimentResult r = ExperimentRunner::run(config, logger);

    std::cout << "\n========== 运行完成 ==========" << std::endl;
    return 0;
}
