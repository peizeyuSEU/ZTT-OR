# Build.mk — 5_重构代码v2
# 碳限额与交易政策下供应链网络设计优化 — GA+BP
#
# 用法：
#   make -f Build.mk              # 编译 ga_bp
#   make -f Build.mk experiments  # 编译 run（实验层启动器）

# CPLEX配置
CPLEX_HOME = /opt/ibm/ILOG/CPLEX_Studio221
CPLEX_INC = $(CPLEX_HOME)/cplex/include
CONCERT_INC = $(CPLEX_HOME)/concert/include
CPLEX_LIB = $(CPLEX_HOME)/cplex/lib/x86-64_linux/static_pic
CONCERT_LIB = $(CPLEX_HOME)/concert/lib/x86-64_linux/static_pic

# 编译器配置
CXX = g++
CXXFLAGS = -std=c++17 -O2 -DIL_STD -I . -I$(CPLEX_INC) -I$(CONCERT_INC)
LDFLAGS = -L$(CPLEX_LIB) -L$(CONCERT_LIB) -lilocplex -lcplex -lconcert -lpthread -ldl -lm

# 源文件
MAIN_SRC = 1_launcher/main_single.cpp
EXPERIMENT_SRC = 0_experiments/main_batch.cpp

MAIN_OBJ = $(MAIN_SRC:.cpp=.o)
EXPERIMENT_OBJ = $(EXPERIMENT_SRC:.cpp=.o)

TARGET = ../../bin/ga_bp
EXPERIMENT_TARGET = ../../bin/batch

# 默认目标
.PHONY: all clean

all: $(TARGET)

experiments: $(EXPERIMENT_TARGET)

# 链接
$(TARGET): $(MAIN_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(EXPERIMENT_TARGET): $(EXPERIMENT_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# 通用编译规则（自动跟踪头文件依赖）
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@
-include $(MAIN_OBJ:.o=.d) $(EXPERIMENT_OBJ:.o=.d)

# 清理
clean:
	rm -f $(MAIN_OBJ) $(EXPERIMENT_OBJ)
	rm -f $(TARGET) $(EXPERIMENT_TARGET)
