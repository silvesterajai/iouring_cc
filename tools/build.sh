#!/usr/bin/env bash

configure_debug() {
    echo "Configuring debug build"
    mkdir -p build/debug
    rm -rf build/debug/CMakeCache.txt
    rm -rf build/debug/CMakeFiles
    cmake -S . -B build/debug -DSANITIZE_ADDRESS:BOOL=ON -DCODE_COVERAGE:BOOL=ON -DUSE_TIMEMORY:BOOL=OFF -DCMAKE_BUILD_TYPE=Debug --log-level=debug -DCMAKE_CXX_FLAGS="-isystem /usr/lib/gcc/x86_64-linux-gnu/11/include/"
}
build_debug() {
   if [[ ! -d build/debug ]]; then
       configure_debug || (echo "Failed to configure" && exit 0)
   fi
   cmake --build build/debug -j 2
   [ "$(readlink -- compile_commands.json)" = `pwd`/build/debug/compile_commands.json ] || {
       rm -f compile_commands.json
       ln -sf `pwd`/build/debug/compile_commands.json .
   }
}

configure_nodebug() {
    echo "Configuring no-debug build"
    mkdir -p build/no-debug
    rm -rf build/no-debug/CMakeCache.txt
    rm -rf build/no-debug/CMakeFiles
    cmake -S . -B build/no-debug -DSANITIZE_ADDRESS:BOOL=OFF -DUSE_TIMEMORY:BOOL=OFF -DCMAKE_BUILD_TYPE=Release --log-level=debug -DCMAKE_CXX_FLAGS="-isystem /usr/lib/gcc/x86_64-linux-gnu/11/include/"
}
build_nodebug() {
    cmake --build build/no-debug -j 2
    [ "$(readlink -- compile_commands.json)" = `pwd`/build/no-debug/compile_commands.json ] || {
       rm -f compile_commands.json
       ln -sf `pwd`/build/no-debug/compile_commands.json .
   }

}


for i in "$@"; do
    "$i"
done

