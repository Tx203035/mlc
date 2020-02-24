./make_osx.sh
./make_ios.sh
./make_android.sh

cp prebuilt/osx/libmlc.dylib ../Assets/mlc-unity/Plugins/macOS/mlc.bundle/Contents/MacOS/mlc
cp prebuilt/ios/libmlc.a ../Assets/mlc-unity/Plugins/iOS
cp prebuilt/android/armeabi-v7a/libmlc.so ../Assets/mlc-unity/Plugins/Android/libs/armeabi-v7a
cp prebuilt/android/arm64-v8a/libmlc.so ../Assets/mlc-unity/Plugins/Android/libs/arm64-v8a
cp prebuilt/android/x86/libmlc.so ../Assets/mlc-unity/Plugins/Android/libs/x86