#!/bin/bash
# build script based on: https://github.com/EpicGamesExt/raddebugger/blob/master/build.sh
set -eu
cd "$(dirname "$0")"

# --- Unpack Arguments --------------------------------------------------------
for arg in "$@"; do declare $arg='1'; done
if [ ! -v gcc ];     then clang=1; fi
if [ ! -v release ]; then debug=1; fi
if [ -v debug ];     then echo "[debug mode]"; fi
if [ -v release ];   then echo "[release mode]"; fi
if [ -v clang ];     then compiler="${CC:-clang}"; echo "[clang compile]"; fi
if [ -v gcc ];       then compiler="${CC:-gcc}"; echo "[gcc compile]"; fi

# --- Unpack Command Line Build Arguments -------------------------------------
auto_compile_flags=''

# --- Compile/Link Line Definitions -------------------------------------------
clang_common='-I../src/ -I../libs/SDL/include/ -I../libs/SDL_image/include/ -I../libs/SDL_net/include/ -g -fdiagnostics-absolute-paths -Wall -Wno-unused-variable -std=c23'
clang_debug="$compiler -O0 -DBUILD_DEBUG=1 ${clang_common} ${auto_compile_flags}"
clang_release="$compiler -O2 -DBUILD_DEBUG=0 ${clang_common} ${auto_compile_flags}"
clang_link="../libs/SDL/build/libSDL3.a ../libs/SDL_image/build/libSDL3_image.a ../libs/SDL_net/build/libSDL3_net.a -lm"
clang_out="-o"
gcc_common='-I../src/ -I../libs/SDL/include/ -I../libs/SDL_image/include/ -I../libs/SDL_net/include/ -g -Wall -Wno-unused-variable -std=c23'
gcc_debug="$compiler -O0 -DBUILD_DEBUG=1 ${gcc_common} ${auto_compile_flags}"
gcc_release="$compiler -O2 -DBUILD_DEBUG=0 ${gcc_common} ${auto_compile_flags}"
gcc_link="../libs/SDL/build/libSDL3.a ../libs/SDL_image/build/libSDL3_image.a ../libs/SDL_net/build/libSDL3_net.a -lm"
gcc_out="-o"

# --- Choose Compile/Link Lines -----------------------------------------------
if [ -v gcc ];     then compile_debug="$gcc_debug"; fi
if [ -v gcc ];     then compile_release="$gcc_release"; fi
if [ -v gcc ];     then compile_link="$gcc_link"; fi
if [ -v gcc ];     then out="$gcc_out"; fi
if [ -v clang ];   then compile_debug="$clang_debug"; fi
if [ -v clang ];   then compile_release="$clang_release"; fi
if [ -v clang ];   then compile_link="$clang_link"; fi
if [ -v clang ];   then out="$clang_out"; fi
if [ -v debug ];   then compile="$compile_debug"; fi
if [ -v release ]; then compile="$compile_release"; fi

# --- Prep Directories --------------------------------------------------------
mkdir -p build

# --- Build Everything (@build_targets) ---------------------------------------
if [ -v sdl ]; then
    didbuild=1
    if [ -d "libs/SDL" ]; then
        # SDL build docs: https://github.com/libsdl-org/SDL/blob/main/docs/README-cmake.md
        # @todo(mg): handle release flag for SDL
        # @todo(mg): pass gcc/clang flag to SDL (is that even possible?)
        cd libs/SDL
        cmake -S . -B build -DSDL_SHARED=OFF -DSDL_STATIC=ON && cmake --build build
        cd ../../
    else
        echo "SDL directory not found! Make sure to initialize git submodules."
    fi
    if [ -d "libs/SDL_image" ]; then
        cd libs/SDL_image
        cmake -S . -B build -DSDLIMAGE_VENDORED=OFF -DBUILD_SHARED_LIBS=OFF "-DSDL3_DIR=..\SDL\build" && cmake --build build
        cd ../../
    else
        echo "SDL_image directory not found! Make sure to initialize git submodules."
    fi
    if [ -d "libs/SDL_net" ]; then
        cd libs/SDL_net
        cmake -S . -B build -DBUILD_SHARED_LIBS=OFF "-DSDL3_DIR=..\SDL\build" && cmake --build build
        cd ../../
    else
        echo "SDL_net directory not found! Make sure to initialize git submodules."
    fi
fi

cd build
if [ -v game ];    then didbuild=1 && $compile ../src/main.c     $compile_link $out demongus; fi
cd ..

# --- Warn On No Builds -------------------------------------------------------
if [ ! -v didbuild ]
then
  echo "[WARNING] no valid build target specified; must use build target names as arguments to this script, like \`./build.sh sdl game\` or \`./build.sh game\`."
  exit 1
fi
