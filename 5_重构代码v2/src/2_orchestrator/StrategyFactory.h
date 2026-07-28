#ifndef STRATEGY_FACTORY_H
#define STRATEGY_FACTORY_H

#include <string>
#include <memory>

/**
 * 策略工厂 — 骨架实现
 *
 * 当前返回默认实现（与原代码行为一致）。
 * 后续可逐步添加具体策略类的实例化逻辑。
 */
class StrategyFactory {
public:
    // 当前所有策略都是硬编码在算法中的默认行为
    // 后续逐步抽取为可插拔策略
};

#endif // STRATEGY_FACTORY_H
