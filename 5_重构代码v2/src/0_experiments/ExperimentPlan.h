#ifndef EXPERIMENT_PLAN_H
#define EXPERIMENT_PLAN_H

#include "../10_common/Config.h"
#include "../10_common/OutputManager.h"
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>

/**
 * 实验方案解析
 *
 * 加载一个 YAML 实验方案文件，支持 base_config 引用，
 * 展开所有实验条目（每个条目是一份完整的 Config）。
 *
 * YAML 格式：
 *   base_config: /path/to/base.yaml
 *   plan_name: "delta_scan"
 *   plan_desc: "扫描delta值"
 *   experiments:
 *     - name: "delta1000"
 *       delta: 1000
 *     - name: "delta3000"
 *       delta: 3000
 */
class ExperimentPlan {
public:
    bool load(const std::string& yamlPath) {
        // 先用 Config 加载整个文件（它支持 experiments 列表）
        Config tempConfig;
        if (!tempConfig.loadFromFile(yamlPath)) {
            std::cerr << "[ExperimentPlan] 无法加载: " << yamlPath << std::endl;
            return false;
        }

        planName_ = tempConfig.plan_name;
        planDesc_ = tempConfig.plan_desc;
        entries_ = tempConfig.experiments;

        // 保存 yaml 顶层公共参数（num_dc/population_size/early_stop 等）。
        // 无 base_config 时，expand() 以它为基准，避免公共参数被默认值覆盖。
        topLevelConfig_ = tempConfig;

        // 检查是否有 base_config 字段（通过重新读取 YAML 手动解析）
        std::ifstream file(yamlPath);
        if (!file.is_open()) return false;

        std::string line;
        while (std::getline(file, line)) {
            // 跳过空行和注释
            if (line.empty() || line[0] == '#') continue;
            // 查找 base_config:
            size_t pos = line.find("base_config:");
            if (pos != std::string::npos) {
                baseConfigPath_ = line.substr(pos + 12);
                // 去除首尾空格和引号
                baseConfigPath_.erase(0, baseConfigPath_.find_first_not_of(" \t\""));
                baseConfigPath_.erase(baseConfigPath_.find_last_not_of(" \t\"") + 1);
                std::filesystem::path basePath(baseConfigPath_);
                if (basePath.is_relative()) {
                    baseConfigPath_ =
                        (std::filesystem::path(yamlPath).parent_path() / basePath)
                            .lexically_normal().string();
                }
                break;
            }
        }
        file.close();

        // 加载 base_config
        if (!baseConfigPath_.empty()) {
            if (!baseConfig_.loadFromFile(baseConfigPath_)) {
                std::cerr << "[ExperimentPlan] 无法加载 base_config: "
                          << baseConfigPath_ << std::endl;
                return false;
            }
        }

        return true;
    }

    /** 展开所有实验条目，每个条目是一份完整 Config */
    std::vector<Config> expand() const {
        std::vector<Config> results;

        for (const auto& entry : entries_) {
            Config cfg;

            if (!baseConfigPath_.empty()) {
                // 优先从 base_config 复制
                cfg = baseConfig_;
            } else {
                // 无 base_config：以 yaml 顶层公共参数为基准，
                // 保证 num_dc/population_size/early_stop 等公共字段被继承，
                // 而不是退化成 Config 的默认值（此前 bug：所有条目都跑默认小规模）。
                cfg = topLevelConfig_;
            }

            // 应用实验条目的覆盖
            cfg.applyEntry(entry);

            // 设置名称
            cfg.plan_name = planName_;

            results.push_back(cfg);
        }

        return results;
    }

    std::string planName() const { return planName_; }
    std::string planDesc() const { return planDesc_; }
    std::string entryName(size_t i) const {
        if (i < entries_.size()) return entries_[i].name;
        return "unknown";
    }
    size_t size() const { return entries_.size(); }

    /** 保存方案配置到文件 */
    void saveConfigTo(const std::string& path) const {
        std::ofstream file(path);
        if (!file.is_open()) return;
        file << "plan_name: " << planName_ << "\n";
        file << "plan_desc: " << planDesc_ << "\n";
        if (!baseConfigPath_.empty()) {
            file << "base_config: " << baseConfigPath_ << "\n";
        }
        file << "experiments: " << entries_.size() << " entries\n";
        for (const auto& e : entries_) {
            file << "  - name: " << e.name << "\n";
            for (const auto& [k, v] : e.overrides) {
                file << "    " << k << ": " << v << "\n";
            }
        }
        file.close();
    }

private:
    std::string baseConfigPath_;
    Config baseConfig_;
    Config topLevelConfig_;   // yaml 顶层公共参数（无 base_config 时作 expand 基准）
    std::vector<ExperimentEntry> entries_;
    std::string planName_;
    std::string planDesc_;
};

#endif // EXPERIMENT_PLAN_H
