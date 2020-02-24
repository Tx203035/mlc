PROJ_ROOT=`pwd`
BUILD_DIR=$PROJ_ROOT/build/android
ANDROID_NDK=/usr/local/android-ndk-r16b

function domake
{
    arch=$1
    PREBUILT_ROOT=$PROJ_ROOT/prebuilt/android/$arch

    mkdir -p $BUILD_DIR/$arch

    (
        cd $BUILD_DIR/$arch
        mkdir -p $PREBUILT_ROOT

        cmake -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
                -DCMAKE_INSTALL_PREFIX=$PREBUILT_ROOT \
                -DANDROID_TOOLCHAIN_NAME=clang \
                -DANDROID_ABI=$arch \
                -DANDROID_NATIVE_API_LEVEL=21 \
                $PROJ_ROOT
    )

    cmake --build $BUILD_DIR/$arch --target install
}

domake armeabi-v7a 
domake arm64-v8a 
domake x86 
