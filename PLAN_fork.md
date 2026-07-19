# 多进程（fork）方案实施计划

## 问题回顾
CPLEX 22.1.0 静态库的全局状态在多线程场景下有 bug，
即使每个线程创建独立 IloEnv 也会冲突（自旋锁死锁）。
多进程（fork）为每个子进程创建完全独立的内存空间和 CPLEX 全局状态，能彻底绕过此问题。

## 设计思路

### 整体架构
```
GA 主进程（父进程）
  │
  ├── fork() → 子进程 #0  → 执行 BP(w[0])  → 结果通过 pipe 传回 → exit
  ├── fork() → 子进程 #1  → 执行 BP(w[1])  → 结果通过 pipe 传回 → exit
  ├── fork() → 子进程 #2  → 执行 BP(w[2])  → 结果通过 pipe 传回 → exit
  │...
  │
  └── waitpid 收集所有子进程结果
```

### 关键设计决策

#### 1. 通信方式：pipe（匿名管道）
- 每个子进程创建一对 pipe（读端 + 写端）
- 子进程将 BP 结果（double fitness）通过 write() 写入 pipe
- 父进程从 pipe 的读端 read() 获取结果
- 优点：简单、可靠、无文件 I/O 开销

#### 2. 子进程需要什么数据
每个子进程需要：
- Instance 的拷贝（完整的深拷贝，因为 fork 后 COW 但 CPLEX 会写内存）
- Config 配置
- w 向量
- Logger 指针（子进程不应继承父进程的 logger 锁）

#### 3. 子进程的限制
- 子进程不能继承父进程的 CPLEX 环境（fork 后 CPLEX 状态不确定）
- 子进程需要**重新创建** BranchAndPrice 对象
- 子进程需要**完整的 Instance 深拷贝**（不能用 COW，因为 CPLEX 内部会修改数据）
- 子进程的 logger 应关闭或只写文件（避免终端输出混乱）

#### 4. 并发控制
- 同时 fork 的子进程数 ≤ `num_threads`（配置参数）
- 用类似信号量的方式控制：同时最多 N 个子进程运行
- 每完成一个，fork 下一个

## 需要修改的文件

### 1. `05_GeneticAlgorithm.h` — 主要修改
**修改 `calculateFitness()` 方法中的并行分支**

新增：
- `#include <sys/wait.h>`, `#include <unistd.h>` 等 POSIX 头文件
- `ForkEvaluator` 辅助类（或内联实现）
- 并行评估的 fork 实现替代当前的 ThreadPool 实现

流程：
```
if (parallel && popSize > 1 && numThreads > 1) {
    // fork 方案
    for each w:
        创建 pipe
        fork()
        if (子进程):
            Instance 深拷贝
            BP 求解
            结果写入 pipe
            _exit(0)
        if (父进程):
            关闭写端，保存读端
    
    // 等待子进程完成
    for each 子进程:
        waitpid 或 wait
        从 pipe 读结果
}
```

### 2. `Instance.h` — 小改
现有的 `clone()` 是浅拷贝（`return *this`），vector 会深拷贝。但对 fork 来说已经够用，**不需要改**。

### 3. `Logger.h` — 小改  
子进程的 logger 应设置 silent 模式（不输出终端），且不继承父进程的 mutex 状态。

### 4. Config — 不需要改
Config 是纯数据 POD，fork 后 COW 没问题。

## 不需要修改的模块
- `BranchAndPrice.h` — 子进程中全新构造，不受父进程影响
- `ColumnGeneration.h`, `BranchAndBound.h` — 同上
- `PricingSolver.h` — 同上
- `Monitor.h` — 子进程不修改 monitor（父进程自己的统计）
- `Orchestrator.h` — 调用 GA.run()，不直接参与并行评估

## 风险评估

| 风险 | 缓解措施 |
|------|---------|
| fork 后 CPLEX 环境继承问题 | 子进程全新构造 BranchAndPrice，不重用父进程的任何 CPLEX 对象 |
| Instance 深拷贝开销 | fork 的 COW 机制 + 子进程只读访问大部分数据，实际拷贝量小 |
| 子进程数控制 | 用简单计数信号量控制同时运行的子进程数 |
| pipe 缓冲区满阻塞 | 每个子进程写少量数据（1个 double），不可能满 |
| 僵尸进程 | 父进程 waitpid 回收所有子进程 |
| 内存泄漏 | 子进程 _exit(0) 后所有内存自动回收 |
| 子进程 stdout 混乱 | 子进程的 logger 设为 silent，BP 不输出进度到终端 |

## 实施步骤

1. 在 GeneticAlgorithm.h 中添加 fork 方案的并行评估代码
2. 修改 Instance.h 添加真正的深拷贝方法（fork-safe）
3. 编译测试 2 个体、1 代验证功能正确
4. 与串行结果对比（同一随机种子下 fitness 是否一致）
5. 跑 30 个体、2 代性能对比（串行 vs fork 并行）
