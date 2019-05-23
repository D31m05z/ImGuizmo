#!/bin/bash -ex

: ${CMAKE_PREFIX_PATH:="$PWD/install"}
: ${PARALLEL:="8"}
: ${GENERATOR:="Unix Makefiles"}

function build {
    echo "----- BUILD AND INSTALL = <$1> -----"
    PACKAGE=$(basename $1)

    cmake -DCMAKE_INSTALL_PREFIX=install -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_FILE -H$1 -Bbuild/$PACKAGE -G "$GENERATOR" "-DCMAKE_BUILD_TYPE=Release" $2
    cmake --build build/$PACKAGE --target install --parallel $PARALLEL
}

# build dependencies
build 3rdparty/glfw
build 3rdparty/imgui "-DIMGUI_IMPL_OPENGL_LOADER_GLEW=true"

# build app
build .
