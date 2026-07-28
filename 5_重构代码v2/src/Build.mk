# Build.mk — 5_重构代码v2
# 碳限额与交易政策下供应链网络设计优化 — GA+BP
#
# 用法：
#   make -f Build.mk              # 编译 ga_bp
#   make -f Build.mk experiments  # 编译 run（实验层启动器）
#   make -f Build.mk run          # 编译并运行（默认配置 delta3000.yaml）

# CPLEX配置
CPLEX_HOME = /home/peizeyu2026/CPLEX_Studio221
CPLEX_INC = $(CPLEX_HOME)/cplex/include
CONCERT_INC = $(CPLEX_HOME)/concert/include
CPLEX_LIB = $(CPLEX_HOME)/cplex/lib/x86-64_linux/static_pic
CONCERT_LIB = $(CPLEX_HOME)/concert/lib/x86-64_linux/static_pic

# 编译器配置
CXX = g++
# 输出根目录：用编译期绝对路径锁定到 v2 根（src 的上一级），
# 使日志固定落到 v2/results，与运行时的当前目录(cwd)完全无关，
# 避免在不同 cwd 下运行导致结果散落到多个位置。
OUTPUT_ROOT = $(abspath ..)
CXXFLAGS = -std=c++17 -O2 -DIL_STD -I . -I$(CPLEX_INC) -I$(CONCERT_INC) -DOUTPUT_ROOT='"$(OUTPUT_ROOT)"' -DCG_DIAG
LDFLAGS = -L$(CPLEX_LIB) -L$(CONCERT_LIB) -lilocplex -lcplex -lconcert -lpthread -ldl -lm

# 源文件
MAIN_SRC = 1_launcher/main_single.cpp
EXPERIMENT_SRC = 0_experiments/main_batch.cpp

MAIN_OBJ = $(MAIN_SRC:.cpp=.o)
EXPERIMENT_OBJ = $(EXPERIMENT_SRC:.cpp=.o)

TARGET = ../bin/ga_bp
EXPERIMENT_TARGET = ../bin/batch

# 默认运行配置（make run 使用），可用命令行覆盖：make run CONFIG=xxx.yaml
CONFIG = 1_launcher/configs/delta3000.yaml

# 默认目标
.PHONY: all clean run experiments all-bins

all: $(TARGET)

experiments: $(EXPERIMENT_TARGET)

# 一次性编译两个产物：单跑 ga_bp + 批量 batch
all-bins: $(TARGET) $(EXPERIMENT_TARGET)

# 编译并运行（默认配置 delta3000.yaml；产物在 v2/bin，日志固定在 v2/results/single）
run: $(TARGET)
	$(TARGET) $(CONFIG)

# 链接（链接前确保产物目录存在）
$(TARGET): $(MAIN_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(EXPERIMENT_TARGET): $(EXPERIMENT_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# 通用编译规则（自动跟踪头文件依赖）
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@
-include $(MAIN_OBJ:.o=.d) $(EXPERIMENT_OBJ:.o=.d)

# 清理
clean:
	rm -f $(MAIN_OBJ) $(EXPERIMENT_OBJ)
	rm -f $(TARGET) $(EXPERIMENT_TARGET)
