#!/bin/bash
total=0

while IFS= read -r file; do
    # 去除//单行注释和/* */多行注释和空行
    count=$(sed '/^[[:space:]]*\/\/.*$/d;/\/\*/,/\*\//d;/^[[:space:]]*$/d' "$file" | wc -l)
    echo "$file: $count 行"
    total=$((total + count))
done < <(find . -name "*.cpp" -o -name "*.h" | grep -v build)

echo "-----------------------------"
echo "有效代码总行数: $total 行"
