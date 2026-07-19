# Build.mk - 碳限额与交易政策下供应链网络设计优化
# 遗传算法 + 分支定价（GA+BP）
#
# 用法：
#   make -f Build.mk              # 编译 ga_bp
#   make -f Build.mk run          # 编译并运行 ga_bp
#   make -f Build.mk experiments  # 编译 run（实验层启动器）
#   make -f Build.mk test-formula # 编译公式测试

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
MAIN_SRC = 1_launcher/main.cpp
EXPERIMENT_SRC = 0_experiments/run.cpp
TEST_FORMULA_SRC = 9_test/test_formula.cpp
TEST_PRICING_SRC = 9_test/test_pricing.cpp

MAIN_OBJ = $(MAIN_SRC:.cpp=.o)
EXPERIMENT_OBJ = $(EXPERIMENT_SRC:.cpp=.o)
TEST_FORMULA_OBJ = $(TEST_FORMULA_SRC:.cpp=.o)
TEST_PRICING_OBJ = $(TEST_PRICING_SRC:.cpp=.o)

TARGET = ../../bin/ga_bp
EXPERIMENT_TARGET = ../../bin/run
TEST_FORMULA_TARGET = ../../bin/test_formula
TEST_PRICING_TARGET = ../../bin/test_pricing

# 默认目标
.PHONY: all clean run

all: $(TARGET)

experiments: $(EXPERIMENT_TARGET)

test-formula: $(TEST_FORMULA_TARGET)
test-pricing: $(TEST_PRICING_TARGET)

# 链接
$(TARGET): $(MAIN_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(EXPERIMENT_TARGET): $(EXPERIMENT_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(TEST_FORMULA_TARGET): $(TEST_FORMULA_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(TEST_PRICING_TARGET): $(TEST_PRICING_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# 通用编译规则
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 运行（工作目录设为 4_重构代码/ 根目录，因为 config.yaml 在那里）
run: $(TARGET)
	cd .. && ./bin/ga_bp

run-experiments: $(EXPERIMENT_TARGET)
	cd .. && ./bin/run

run-test-formula: $(TEST_FORMULA_TARGET)
	cd .. && ./bin/test_formula

run-test-pricing: $(TEST_PRICING_TARGET)
	cd .. && ./bin/test_pricing

# 清理
clean:
	rm -f $(MAIN_OBJ) $(EXPERIMENT_OBJ) $(TEST_FORMULA_OBJ) $(TEST_PRICING_OBJ)
	rm -f $(TARGET) $(EXPERIMENT_TARGET) $(TEST_FORMULA_TARGET) $(TEST_PRICING_TARGET)
	rm -rf ../results/
