# 顶层 Makefile — 封装 src/Build.mk
#
#   make              # 编译 ga_bp（单次运行）
#   make run          # 编译并运行
#   make clean        # 清理

.PHONY: all run clean

all:
	$(MAKE) -C src -f Build.mk all

run: all
	../bin/ga_bp

experiments:
	$(MAKE) -C src -f Build.mk experiments

clean:
	$(MAKE) -C src -f Build.mk clean
