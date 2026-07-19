#ifndef OUTPUT_MANAGER_H
#define OUTPUT_MANAGER_H

#include <string>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <sys/stat.h>

/**
 * 输出目录管理器
 *
 * 负责创建带时间戳的输出子文件夹，每次运行独立目录，不会被覆盖。
 * 单次运行 → results_single/YYYYMMDD_HHmmss_配置名/
 * 批量实验 → results_experiment/YYYYMMDD_HHmmss_方案名/
 */
class OutputManager {
public:
    /** 为单次运行创建输出目录 */
    static std::string createSingleRunDir(const std::string& configName) {
        std::string dir = "results_single/" + timestamp() + "_" + configName + "/";
        mkdir("results_single", 0755);
        mkdir(dir.c_str(), 0755);
        return dir;
    }

    /** 为批量实验创建输出目录 */
    static std::string createExperimentDir(const std::string& planName) {
        std::string dir = "results_experiment/" + timestamp() + "_" + planName + "/";
        mkdir("results_experiment", 0755);
        mkdir(dir.c_str(), 0755);
        return dir;
    }

    /** 在批量实验目录下为单个条目创建子目录 */
    static std::string createEntryDir(const std::string& parentDir,
                                       const std::string& entryName) {
        std::string dir = parentDir + entryName + "/";
        mkdir(dir.c_str(), 0755);
        return dir;
    }

private:
    static std::string timestamp() {
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        std::stringstream ss;
        ss << std::put_time(&tm, "%Y%m%d_%H%M%S");
        return ss.str();
    }
};

#endif // OUTPUT_MANAGER_H
