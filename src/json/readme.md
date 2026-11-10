rmdir /s /q build
cmake -S src/json -B build -A Win32
cmake --build build --config Release


funding