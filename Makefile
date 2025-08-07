# Makefile for C++ HTTP server

# Compiler
CXX = g++

# Compiler flags
# -std=c++17: 使用 C++17 标准
# -Wall: 开启所有警告
# -Wextra: 开启额外警告
# -g: 包含调试信息
# -O2: 优化级别2，适合发布版本
CXXFLAGS = -std=c++17 -Wall -Wextra -g

# Linker flags
# -lrt: 链接实时库（可能需要用于某些系统调用，尽管在此代码中可能不是必需的）
# -lpthread: 如果你打算实现多线程，这是必需的
# -lstdc++: 明确链接C++标准库（通常g++会自动完成）
LDFLAGS = -lstdc++

# Source files and object files
SRCS = server.cpp
OBJS = $(SRCS:.cpp=.o)

# Executable name
TARGET = tiny_server

# The default target
all: $(TARGET)

# Rule to link the executable
$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) $(OBJS) -o $(TARGET)

# Rule to compile C++ source files into object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to clean up generated files
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets to prevent conflicts with file names
.PHONY: all clean