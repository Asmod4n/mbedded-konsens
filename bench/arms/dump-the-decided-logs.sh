set -uo pipefail
cmake -S /src -B /work/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-16 -DCMAKE_C_COMPILER=gcc-16 -Wno-dev > /work/cmake.log 2>&1 || { tail -30 /work/cmake.log; exit 1; }
cmake --build /work/build -j2 --target test_simulation test_simulation_clients test_simulation_rollback > /work/build.log 2>&1 || { grep -E 'error|Error' /work/build.log | head -30; exit 1; }
cd /work/build
for t in test_simulation test_simulation_clients test_simulation_rollback; do mkdir -p /work/d/$t; DUMP_DIR=/work/d/$t ./$t > /work/$t.log 2>&1; echo "$t rc=$?"; tail -2 /work/$t.log; done
