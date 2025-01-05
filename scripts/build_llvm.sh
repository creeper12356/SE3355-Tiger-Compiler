#!/bin/bash

WORKDIR=$(dirname "$(dirname "$(readlink -f "$0")")")

# 检查参数数量
if [ "$#" -ne 1 ]; then
    echo "用法: \$0 <llvm_file.ll> "
    exit 1
fi

LLVM_FILE=$1
C_FILE=${WORKDIR}/src/tiger/runtime/runtime_llvm.c

# 检查文件是否存在
if [ ! -f "$LLVM_FILE" ]; then
    echo "错误: 找不到文件 $LLVM_FILE"
    exit 1
fi

if [ ! -f "$C_FILE" ]; then
    echo "错误: 找不到文件 $C_FILE"
    exit 1
fi


# 编译 LLVM IR 文件
clang -target x86_64-pc-linux-gnu -g "$LLVM_FILE" -S -o module.s
if [ $? -ne 0 ]; then
    echo "编译 LLVM 文件失败"
    exit 1
fi

clang -S "$C_FILE" -o runtime_llvm.s

# 编译 C 文件
clang -target x86_64-pc-linux-gnu -g "$C_FILE" module.s -o test_program
if [ $? -ne 0 ]; then
    echo "编译 C 文件失败"
    exit 1
fi

