CXX := g++
CXXFLAGS := -std=c++20 -Wall -g
INCLUDES := -I./common -I./database -I./http -I./net

# webserver 源码
SRC_DIRS := common database http net
SRCS := $(wildcard $(addsuffix /*.cpp, $(SRC_DIRS))) main.cpp server.cpp
OBJS := $(SRCS:.cpp=.o)

# 测试源码和目标
TEST_DIR := test
TEST_CONFIG_SRC := $(TEST_DIR)/test_config.cpp common/config.cpp common/util.cpp
TEST_LOG_SRC := $(TEST_DIR)/test_log.cpp common/log.cpp
TEST_UTIL_SRC := $(TEST_DIR)/test_util.cpp common/util.cpp
TEST_CONFIG_BIN := $(TEST_DIR)/test_config
TEST_LOG_BIN := $(TEST_DIR)/test_log
TEST_UTIL_BIN := $(TEST_DIR)/test_util


# 输出目标
TARGET := webserver

.PHONY: all clean test run_config run_util

# 编译全部
all: $(TARGET) $(TEST_CONFIG_BIN) $(TEST_UTIL_BIN) $(TEST_LOG_BIN)

# 主程序
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

# 测试程序
$(TEST_CONFIG_BIN): $(TEST_CONFIG_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

$(TEST_UTIL_BIN): $(TEST_UTIL_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

$(TEST_LOG_BIN): $(TEST_LOG_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

# 运行全部测试
test: $(TEST_CONFIG_BIN) $(TEST_UTIL_BIN) $(TEST_LOG_BIN)
	@echo "Running test_config..."
	./$(TEST_CONFIG_BIN)
	@echo "Running test_util..."
	./$(TEST_UTIL_BIN)
	@echo "Running test_util..."
	./$(TEST_LOG_BIN)

# 单独运行某个测试
config: $(TEST_CONFIG_BIN)
	./$(TEST_CONFIG_BIN)

util: $(TEST_UTIL_BIN)
	./$(TEST_UTIL_BIN)

log: $(TEST_LOG_BIN)
	./$(TEST_LOG_BIN)

# 清理
clean:
	rm -f $(OBJS) $(TARGET) $(TEST_CONFIG_BIN) $(TEST_UTIL_BIN) $(TEST_LOG_BIN)
