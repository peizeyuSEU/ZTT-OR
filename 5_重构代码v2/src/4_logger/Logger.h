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
#include <mutex>
#include <thread>

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
/**
 * 日志输出工具
 *
 * ===== 输出设计原则 =====
 * 1. "progress" 方法：只输出关键进度信息，回答"当前在哪个阶段、进度如何、关键数值是多少"
 * 2. "log" 方法：默认只输出到文件，用于调试
 * 3. 不再在每个DC、每个节点处输出详细过程
 * 4. 所有输出都写文件（run.log），终端只输出 progress 和重要事件
 */
class Logger {
private:
    std::ofstream logFile;
    bool verbose = false;
    bool silent = false;          // 静默模式：只写文件，不输出终端
    bool consoleMode_ = true;     // true=终端输出progress, false=只写文件
    std::chrono::steady_clock::time_point startTime;
    int currentLevel = 0;  // 当前层级 0=GA, 1=BP, 2=CG/BB, 3=PS
    std::mutex logMutex;  // 线程安全

public:
    Logger() {
        startTime = std::chrono::steady_clock::now();
    }

    /** 显式刷盘并关闭文件。用于 fork 子进程在 _exit 前保证日志落盘
     *  （_exit 不调用析构，也不 flush C++ 流缓冲，需手动调用） */
    void close() {
        if (logFile.is_open()) {
            logFile.flush();
            logFile.close();
        }
    }

    void setSilent(bool s) { silent = s; }
    bool isSilent() const { return silent; }
    void setConsoleMode(bool on) { consoleMode_ = on; }
    bool getConsoleMode() const { return consoleMode_; }

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
     * 按层级输出信息日志（线程安全）
     *
     * 默认行为：写文件，不输出终端。
     * 终端输出请用 progress() 方法。
     *
     * @param level 层级（0=GA, 1=BP, 2=CG/BB, 3=PS）
     * @param msg 日志消息
     */
    void log(int level, const std::string& msg) {
        std::lock_guard<std::mutex> lock(logMutex);
        std::string indent = getIndent(level);
        std::string prefix = getPrefix(level);
        std::string line = indent + prefix + " " + msg;
        // 只在非 silent 且 consoleMode_ 时输出终端（默认 log 不输出终端）
        if (!silent && consoleMode_) {
            std::cout << line << std::endl;
        }
        if (logFile.is_open()) {
            logFile << line << std::endl;
        }
    }

    /**
     * 输出关键进度信息（线程安全）
     *
     * 这个方法是设计用来输出到终端的——回答"当前在哪个阶段、进度如何、关键数值是多少"
     * 与 log() 不同，它在终端和文件中都会输出。
     *
     * @param level 层级
     * @param msg 进度消息（简洁、有信息量）
     */
    void progress(int level, const std::string& msg) {
        std::lock_guard<std::mutex> lock(logMutex);
        std::string indent = getIndent(level);
        std::string prefix = getPrefix(level);
        std::string line = indent + prefix + " " + msg;
        // 进度日志必然输出终端（除非 silent）
        if (!silent) {
            std::cout << line << std::endl;
        }
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

    /**
     * 原样写入一行（不加层级前缀/缩进）。
     *
     * 用于写入已排版好的多行报表（如时间分布树），
     * 保持在 run.log 中的对齐格式。终端是否输出由 consoleMode_ 控制。
     */
    void raw(const std::string& msg) {
        std::lock_guard<std::mutex> lock(logMutex);
        if (!silent && consoleMode_) {
            std::cout << msg << std::endl;
        }
        if (logFile.is_open()) {
            logFile << msg << std::endl;
        }
    }
};

#endif // LOGGER_H
