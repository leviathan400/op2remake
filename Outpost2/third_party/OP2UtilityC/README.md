# OP2UtilityC (prebuilt)

The C-ABI wrapper of OP2Utility that `op2maps` uses to read `.map` files.

| | |
|---|---|
| Source | https://github.com/leviathan400/OP2UtilityC at `c9bb829` ("Set DLL version to 1.2.0.0") |
| Version | DLL 1.2.0.0, C ABI version 2 (`op2_capi_version()`) |
| Built with | Qt's MinGW 13.1.0 64-bit, CMake + Ninja, Release; C++ runtime linked statically, so it needs only `KERNEL32` and `msvcrt` |
| Files | `OP2Utility.dll`, `libOP2Utility.dll.a` (the MinGW import library `maps/CMakeLists.txt` links), `op2utility_c.h` (the repo's `capi/op2utility_c.h`) |

To rebuild: clone the repo, then

```
cmake -S OP2UtilityC -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
build\op2utility_smoketest.exe
```

with `C:\Qt\Tools\mingw1310_64\bin`, `C:\Qt\Tools\CMake_64\bin` and `C:\Qt\Tools\Ninja` on `PATH`,
and copy `OP2Utility.dll`, `libOP2Utility.dll.a` and `capi/op2utility_c.h` here.
