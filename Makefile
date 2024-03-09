# 定义输出目录
OUTPUT_DIR := output
# 定义目标文件
TARGET := $(OUTPUT_DIR)/smallchat_server

CXXFLAGS := -g -Wall -O0 -std=c++17

$(TARGET): smallchat_server.cc | $(OUTPUT_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(OUTPUT_DIR) :
	mkdir -p $@

clean:
	rm -rf output

.PHONY = clean