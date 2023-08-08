## Building
```
cd build
cmake ..
cmake --build .
```

## Usage

clang -O1 main.c -fpass-plugin=../obfus/build/libHelloWorld.so -o main
