CXX_FLAGS=-std=c++17 -Wall -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization -Wformat=2 -Winit-self  -Wmissing-include-dirs -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wshadow -Wsign-conversion -Wsign-promo -Wstrict-overflow=5 -Wswitch-default -Wundef -Werror -Wno-unused
CXX_RELEASE_FLAGS=-O3
CXX_DEBUG_FLAGS=-g #-fsanitize=undefined -fsanitize=address 
CC=clang++

default: compiler-debug

compiler: compiler.cpp
	$(CC) $(CXX_FLAGS) $(CXX_RELEASE_FLAGS) compiler.cpp -o compiler.out

compiler-debug: compiler.cpp
	$(CC) $(CXX_FLAGS) $(CXX_DEBUG_FLAGS) compiler.cpp -o compiler.out

interpreter: interpreter.cpp
	$(CC) $(CXX_FLAGS) $(CXX_RELEASE_FLAGS) interpreter.cpp -o interpreter.out

interpreter-debug: interpreter.cpp
	$(CC) $(CXX_FLAGS) $(CXX_DEBUG_FLAGS) interpreter.cpp -o interpreter.out