call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"

set PROJ_ROOT=%CD%
set BUILD_DIR=%CD%\build\win
set PREBUILT_DIR=%CD%\prebuilt\win

mkdir %BUILD_DIR% & pushd %BUILD_DIR%
cmake -G "Visual Studio 15 2017 Win64" -T "LLVM" -DCMAKE_INSTALL_PREFIX=%PREBUILT_DIR% -DCMAKE_BUILD_TYPE=Debug %PROJ_ROOT%
popd

cmake --build %BUILD_DIR% --config Debug --target install