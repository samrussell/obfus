# obfus

This is a proof of concept control flow flattener as an LLVM pass

## Building

```
cd make
cmake ..
cmake --build .
```

## Usage

```
clang main.c -fpass-plugin=../obfus/build/libcff.so -o main
```
