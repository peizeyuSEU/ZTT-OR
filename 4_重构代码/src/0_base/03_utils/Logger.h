#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <vector>

/**
 * 日志输出工具
 * 支持层级缩进和层级前缀标识
 *
 * 层级定义（论文第5节）：
 *   Level 4 [GA] - 遗传算法，缩进0空格
 *   Level 3 [BP] - 分支定价，缩进2空格
 *   Level 2 [CG] - 列生成，缩进4空格
 *   Level 2 [BB] - 分支定界，缩进4空格
 *   Level 1 [PS] - 定价子问题，缩进6空格
 */
class Logger {
private:
    std::ofstream logFile;
    bool verbose = false;
    std::chrono::steady_clock::time_point startTime;
    int currentLevel = 0;  // 当前层级 0=GA, 1=BP, 2=CG/BB, 3=PS

public:
    Logger() {
        startTime = std::chrono::steady_clock::now();
    }

    ~Logger() {
        if (logFile.is_open()) {
            logFile.close();
        }
    }

    bool init(const std::string& filename, bool verboseMode = false) {
        verbose = verboseMode;
        if (!filename.empty()) {
            logFile.open(filename);
            if (!logFile.is_open()) {
                std::cerr << "[Logger] 无法打开日志文件: " << filename << std::endl;
                return false;
            }
        }
        return true;
    }

    /** 设置当前层级 */
    void setLevel(int level) { currentLevel = level; }
    int getLevel() const { return currentLevel; }

    /** 获取层级前缀 */
    static std::string getPrefix(int level) {
        switch (level) {
            case 0: return "[GA]";
            case 1: return "[BP]";
            case 2: return "[CG]";
            case 3: return "[BB]";
            case 4: return "[PS]";
            default: return "[??]";
        }
    }

    /** 获取层级缩进 */
    static std::string getIndent(int level) {
        return std::string(std::min(level, 4) * 2, ' ');
    }

    static std::string getTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    double getElapsedSeconds() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - startTime).count();
    }

    /**
     * 按层级输出信息日志
     * @param level 层级（0=GA, 1=BP, 2=CG/BB, 3=PS）
     * @param msg 日志消息
     */
    void log(int level, const std::string& msg) {
        std::string indent = getIndent(level);
        std::string prefix = getPrefix(level);
        std::string line = indent + prefix + " " + msg;
        std::cout << line << std::endl;
        if (logFile.is_open()) {
            logFile << line << std::endl;
        }
    }

    /** 在当前层级输出 */
    void info(const std::string& msg) {
        log(currentLevel, msg);
    }

    void debug(const std::string& msg) {
        if (!verbose) return;
        log(currentLevel, "[DEBUG] " + msg);
    }

    void warn(const std::string& msg) {
        log(currentLevel, "[WARN] " + msg);
    }

    void error(const std::string& msg) {
        std::string indent = getIndent(currentLevel);
        std::string line = indent + "[ERROR] " + msg;
        std::cerr << line << std::endl;
        if (logFile.is_open()) {
            logFile << line << std::endl;
        }
    }

    void separator() {
        std::string line = "========================================";
        std::cout << line << std::endl;
        if (logFile.is_open()) {
            logFile << line << std::endl;
        }
    }
};

#endif // LOGGER_H
