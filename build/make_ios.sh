PROJ_ROOT=`pwd`
BUILD_DIR=$PROJ_ROOT/build/ios
PREBUILT_DIR=$PROJ_ROOT/prebuilt/ios

mkdir -p $BUILD_DIR

(
    cd $BUILD_DIR
    mkdir -p $PREBUILT_DIR

    cmake -G Xcode \
            -DCMAKE_INSTALL_PREFIX=$PREBUILT_DIR \
            -DCMAKE_TOOLCHAIN_FILE=$PROJ_ROOT/ios.toolchain.cmake \
            -DIOS_PLATFORM=OS64 \
            $PROJ_ROOT
)

cmake --build $BUILD_DIR --target install