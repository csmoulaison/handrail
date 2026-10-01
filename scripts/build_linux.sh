EXE=$1
TARGET=$2
DEBUG_OR_RELEASE=$3

# Common arguments to static and dynamic builds
FLAGS="-std=c99 -Wall -Werror -Wno-unused "
INCLUDE="-I handrail/code/ -I code/ -I handrail/extern/ "
#SRC="build/asset/pack.o "

case $DEBUG_OR_RELEASE in
    "debug")
        FLAGS+="-g -rdynamic "
        ;;
    "release")
        FLAGS+="-O2 "
        ;;
esac

mkdir -p bin
mkdir -p build
mkdir -p build/asset
mkdir -p code/generated

error_on_exit() {
    if [[ $? -ne 0 ]]; then
        printf "Build was \033[1m\033[31munsuccessful\033[0m\n"
        exit 1
    fi
}

start_step() {
    printf "%s..." "$1"
}

end_step() {
    error_on_exit
    printf "\033[32m\033[1mDone!\033[0m\n"
}

prebuild() {
    start_step "Prebuild"
    if [[ -f "build/build" ]]; then
        ./build/build
        if [[ $? -ne 0 ]]; then error_on_exit; fi
    fi
    end_step
}

# Prebuild rasterizes fonts with the vendored FreeType. Build it once into
# handrail/extern/freetype/out, static and without optional dependencies.
FREETYPE_DIR="handrail/extern/freetype"
FREETYPE_OUT="$FREETYPE_DIR/out"

freetype() {
    if [[ -f "$FREETYPE_OUT/lib/libfreetype.a" ]]; then
        return
    fi
    start_step "FreeType"
    cmake -S $FREETYPE_DIR -B build/freetype \
        -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
        -DCMAKE_INSTALL_PREFIX="$PWD/$FREETYPE_OUT" -DCMAKE_INSTALL_LIBDIR=lib \
        -DFT_DISABLE_ZLIB=TRUE -DFT_DISABLE_BZIP2=TRUE -DFT_DISABLE_PNG=TRUE \
        -DFT_DISABLE_HARFBUZZ=TRUE -DFT_DISABLE_BROTLI=TRUE > build/freetype.log \
    && cmake --build build/freetype --parallel >> build/freetype.log \
    && cmake --install build/freetype >> build/freetype.log
    end_step
}

bootstrap() {
    freetype

    start_step "Bootstrap"
    gcc code/prebuild.c -o build/build $INCLUDE -I $FREETYPE_OUT/include/freetype2 -g -rdynamic -Wall -Werror -Wno-unused \
        $FREETYPE_OUT/lib/libfreetype.a -lm
    end_step
}

static() {
    prebuild

    start_step "Static (asset link)"
    ld -r -b binary build/asset/pack.data -o build/asset/pack.o
    end_step

    start_step "Static (build)"
    gcc handrail/code/main.c build/asset/pack.o \
        -o bin/$EXE \
        $INCLUDE \
        $FLAGS \
        -DGAME_NAME="\"$EXE\"" -DGAME_LIB_NAME="\"$EXE.so\"" \
        -lasound -lX11 -lm -ldl
    end_step
}

dynamic() {
    start_step "Game library (intermediate)"
    gcc code/game.c \
        -o build/${EXE}.o \
        $INCLUDE \
        $FLAGS -c -fPIC
    end_step

    start_step "Game library (final)"
    gcc build/${EXE}.o \
        -o bin/${EXE}_tmp.so \
        $INCLUDE \
        $FLAGS -shared -lm
    end_step

    mv bin/${EXE}_tmp.so bin/${EXE}.so
}

case "$TARGET" in
    "bootstrap")
        bootstrap
        ;;
    "static")
        static
        ;;
    "dynamic")
        dynamic
        ;;
    "all")
        bootstrap
        static
        dynamic
        ;;
    "clean")
        rm -rf bin/*
        rm -rf build/*
        exit 0
        ;;
esac

printf "Build was \033[1m\033[32msuccessful\033[0m\n"
