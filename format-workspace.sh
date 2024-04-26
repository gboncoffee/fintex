#!/bin/bash

for file in $(find . -name "*.c")
do
	clang-format -i $file
done

for file in $(find . -name "*.h")
do
	clang-format -i $file
done
