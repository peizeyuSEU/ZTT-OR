#ifndef OUTPUT_MANAGER_H
#define OUTPUT_MANAGER_H

#include <string>
#include <ctime>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <sys/stat.h>
#include <cerrno>
#include <stdexcept>

// 输出根目录：由编译期宏 OUTPUT_ROOT 指定（Build.mk 传入 v2 绝对/相对路径），
// 未定义时退回当前工作目录，保证任意目录运行都固定落到同一处。
#ifndef OUTPUT_ROOT
#define OUTPUT_ROOT ""
#endif

/**
 * 输出目录管理器
 *
 * 负责创建带时间戳的输出子文件夹，每次运行独立目录，不会被覆盖。
 * 单次运行 → results/single/YYYYMMDD_HHmmss_配置名/
 * 批量实验 → results/experiment/YYYYMMDD_HHmmss_方案名/
 */
class OutputManager {
public:
    /** 为单次运行创建输出目录 */
    static std::string createSingleRunDir(const std::string& configName) {
        std::string root = rootPrefix();
        mkdir((root + "results").c_str(), 0755);
        std::string base = root + "results/single";
        mkdir(base.c_str(), 0755);
        return createUniqueDir(base, configName);
    }

    /** 为批量实验创建输出目录 */
    static std::string createExperimentDir(const std::string& planName) {
        std::string root = rootPrefix();
        mkdir((root + "results").c_str(), 0755);
        std::string base = root + "results/experiment";
        mkdir(base.c_str(), 0755);
        return createUniqueDir(base, planName);
    }

    /** 在批量实验目录下为单个条目创建子目录 */
    static std::string createEntryDir(const std::string& parentDir,
                                       const std::string& entryName) {
        std::string dir = parentDir + entryName + "/";
        mkdir(dir.c_str(), 0755);
        return dir;
    }

private:
    // 返回输出根目录前缀（保证以 '/' 结尾且目录已创建）；OUTPUT_ROOT 为空时返回空串
    static std::string rootPrefix() {
        std::string root = OUTPUT_ROOT;
        if (root.empty()) return "";
        if (root.back() != '/') root += '/';
        mkdir(root.c_str(), 0755);
        return root;
    }

    static std::string timestamp() {
        const auto now = std::chrono::system_clock::now();
        const auto inTimeT = std::chrono::system_clock::to_time_t(now);
        const auto tm = *std::localtime(&inTimeT);
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count() % 1000;
        std::stringstream ss;
        ss << std::put_time(&tm, "%Y%m%d_%H%M%S")
           << "_" << std::setw(3) << std::setfill('0') << millis;
        return ss.str();
    }

    static std::string createUniqueDir(const std::string& base,
                                       const std::string& label) {
        const std::string safeLabel = label.empty() ? "unnamed" : label;
        const std::string stem = base + "/" + timestamp() + "_" + safeLabel;
        for (int suffix = 0; suffix < 10000; ++suffix) {
            const std::string dir =
                stem + (suffix == 0 ? "" : "_" + std::to_string(suffix)) + "/";
            if (mkdir(dir.c_str(), 0755) == 0) return dir;
            if (errno != EEXIST) {
                throw std::runtime_error("Failed to create output directory: " + dir);
            }
        }
        throw std::runtime_error("Unable to allocate a unique output directory");
    }
};

#endif // OUTPUT_MANAGER_H
