CXX_FLAGS=-Werror -Wall -Wextra -Wpedantic -Wshadow -std=c++20 -Wundef
CXX_RELEASE_FLAGS=-O3
CXX_DEBUG_FLAGS=-fsanitize=undefined -fsanitize=address -Og
CC=clang++

default: compiler-debug

compiler: compiler.cpp
	$(CC) $(CXX_FLAGS) $(CXX_RELEASE_FLAGS) compiler.cpp -o compiler.out

compiler-debug: compiler.cpp
	$(CC) $(CXX_FLAGS) $(CXX_DEBUG_FLAGS) compiler.cpp -o compiler.out