# 顶层 Makefile — 封装 src/Build.mk
#
#   make              # 编译 ga_bp（单次运行）
#   make run          # 编译并运行
#   make clean        # 清理
#   make release      # 清理并构建正式版（默认关闭逐轮 CG 诊断）
#   make diagnostics  # 清理并构建诊断版（启用逐轮 CG 诊断）

.PHONY: all run clean release diagnostics

all:
	$(MAKE) -C src -f Build.mk all

run: all
	../bin/ga_bp

experiments:
	$(MAKE) -C src -f Build.mk experiments

release:
	$(MAKE) -C src -f Build.mk release

diagnostics:
	$(MAKE) -C src -f Build.mk diagnostics

clean:
	$(MAKE) -C src -f Build.mk clean
