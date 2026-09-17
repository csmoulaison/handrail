EXE=$1
TARGET=$2
DEBUG_OR_RELEASE=$3

# Common arguments to static and dynamic builds
FLAGS="-std=c99 -Wall -Werror -Wno-unused "
INCLUDE="-I code/ -I extern/ "
#SRC="build/asset/pack.o "

case $DEBUG_OR_RELEASE in
    "debug")
        FLAGS+="-g -rdynamic "
        ;;
    "release")
        FLAGS+="-02 "
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

bootstrap() {
    start_step "Bootstrap"
	gcc code/build.c -o build/build -g -rdynamic -Wall -Werror -Wno-unused -lm
    end_step
}

static() {
    prebuild

    start_step "Static (asset link)"
    ld -r -b binary build/asset/pack.data -o build/asset/pack.o
    end_step

    start_step "Static (build)"
    gcc code/csm_core/main.c extern/GL/gl3w.c build/asset/pack.o \
        -o bin/$EXE \
        $INCLUDE \
        $FLAGS \
        -DGAME_NAME="\"$EXE\"" -DGAME_LIB_NAME="\"$EXE.so\"" \
        -lasound -lX11 -lX11-xcb -lGL -lm -lxcb -lXfixes
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
