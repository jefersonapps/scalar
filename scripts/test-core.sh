#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")/.."
mkdir -p build-core
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -Isrc src/documents/Document.cpp src/commands/History.cpp src/rendering/StrokeMesh.cpp src/rendering/BackgroundMesh.cpp src/persistence/ZipArchive.cpp src/geometry/Geometry.cpp tests/core_tests.cpp -o build-core/core_tests
./build-core/core_tests build-core/fixture.board
