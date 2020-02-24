PROJ_ROOT=`pwd`
BUILD_DIR=$PROJ_ROOT/build/osx
PREBUILT_DIR=$PROJ_ROOT/prebuilt/osx

mkdir -p $BUILD_DIR

(
    cd $BUILD_DIR
    mkdir -p $PREBUILT_DIR

    cmake -G Xcode \
            -DCMAKE_INSTALL_PREFIX=$PREBUILT_DIR \
            $PROJ_ROOT
)

cmake --build $BUILD_DIR --config Debug --target install