# ============================================================
# 碳限额与交易政策下供应链网络设计优化
# 遗传算法 + 分支定价（GA+BP）
#
# 顶层 Makefile — 封装 src/Build.mk
# 在项目根目录（4_重构代码/）下运行：
#
#   make              # 编译 ga_bp（单次运行）
#   make run          # 编译并运行（默认 3 DC, 10 Rts）
#   make experiments  # 编译 run（批量实验）
#   make table4_1     # 编译并运行表4.1实验
#   make table4_2     # 编译并运行表4.2实验
#   make verify       # 编译并运行验证实验
#   make test-formula # 编译并运行公式验证
#   make test-pricing # 编译并运行定价子问题算法对比测试
#   make clean        # 清理
# ============================================================

.PHONY: all run experiments table4_1 table4_2 verify test-formula test-pricing clean

# 所有工作委托给 src/Build.mk
all:
	$(MAKE) -C src -f Build.mk all

run: all
	../bin/ga_bp

experiments:
	$(MAKE) -C src -f Build.mk experiments

table4_1: experiments
	../bin/run src/0_experiments/table4_1.yaml

table4_2: experiments
	../bin/run src/0_experiments/table4_2.yaml

verify: experiments
	../bin/run src/0_experiments/verify.yaml

test-formula:
	$(MAKE) -C src -f Build.mk all
	$(MAKE) -C src -f Build.mk test-formula
	../bin/test_formula

test-pricing:
	$(MAKE) -C src -f Build.mk all
	$(MAKE) -C src -f Build.mk test-pricing
	../bin/test_pricing

clean:
	$(MAKE) -C src -f Build.mk clean
