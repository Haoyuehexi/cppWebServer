# 编译器设置
CXX = g++
CXXFLAGS = -std=c++11 -O2 -Wall -g

# 包含目录
INCLUDES = -I. -I./database -I./http -I./log -I./threadpool -I./timer

# 库文件链接
LIBS = -lpthread -lmysqlclient -L/usr/lib64/mysql

# 目标文件名
TARGET = webserver

# 源文件目录
SRC_DIRS = . ./database ./http ./log ./timer

# 查找所有源文件
SOURCES = $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.cpp))

# 默认目标
all: $(TARGET)

# 直接从源文件生成可执行文件
$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)
	@echo "Build complete: $(TARGET)"

# 清理编译生成的文件
clean:
	rm -f $(TARGET)
	@echo "Clean complete"

# 清理并重新编译
rebuild: clean all

# 运行程序
run: $(TARGET)
	./$(TARGET)

# 调试编译
debug: CXXFLAGS += -DDEBUG -g3
debug: $(TARGET)

# 发布编译
release: CXXFLAGS += -DNDEBUG -O3
release: clean $(TARGET)

# 检查代码风格（需要安装cpplint）
lint:
	@if command -v cpplint >/dev/null 2>&1; then \
		find . -name "*.cpp" -o -name "*.h" | xargs cpplint; \
	else \
		echo "cpplint not found. Please install it first."; \
	fi

# 格式化代码（需要安装clang-format）
format:
	@if command -v clang-format >/dev/null 2>&1; then \
		find . -name "*.cpp" -o -name "*.h" | xargs clang-format -i; \
		echo "Code formatted"; \
	else \
		echo "clang-format not found. Please install it first."; \
	fi

# 显示帮助信息
help:
	@echo "Available targets:"
	@echo "  all       - Build the project (default)"
	@echo "  clean     - Remove all build files"
	@echo "  rebuild   - Clean and build"
	@echo "  run       - Build and run the program"
	@echo "  debug     - Build with debug flags"
	@echo "  release   - Build optimized release version"
	@echo "  lint      - Check code style"
	@echo "  format    - Format source code"
	@echo "  help      - Show this help message"

# 声明伪目标
.PHONY: all clean rebuild install uninstall run debug release lint format help