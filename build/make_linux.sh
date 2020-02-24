PROJ_ROOT=`pwd`
BUILD_DIR=$PROJ_ROOT/build/linux
PREBUILT_DIR=$PROJ_ROOT/prebuilt/linux

mkdir -p $BUILD_DIR

(
    cd $BUILD_DIR
    mkdir -p $PREBUILT_DIR

    cmake -DCMAKE_INSTALL_PREFIX=$PREBUILT_DIR \
            $PROJ_ROOT
)

cmake --build $BUILD_DIR --target install

#rm /usr/lib64/python2.7/site-packages/mlc_py.so
#ln -s $PREBUILT_DIR/mlc_py.so /usr/lib64/python2.7/site-packages
