#!/bin/bash

# directories to be formatted (recursive search)
DIRS="QTerrainRenderer/include QTerrainRenderer/src tests"
FORMAT_CMD="clang-format -style=file:scripts/clang_style -i"

echo "- clang-format"

for D in ${DIRS}; do
    for F in `find ${D}/. -type f \( -iname \*.hpp -o -iname \*.cpp \)`; do
	echo ${F}
	${FORMAT_CMD} ${F}
    done
done

echo "- cmake-format"

cmake-format -i CMakeLists.txt
cmake-format -i QTerrainRenderer/CMakeLists.txt
