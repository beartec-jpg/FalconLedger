# qXRP Fuzz Targets

Three libFuzzer targets are provided in this directory.

## Prerequisites

```bash
# Install clang with fuzzer support
sudo apt install clang lld

# Build xrpl library first (see repo root README)
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -Dxrpld=OFF -Dtests=OFF \
      -DCMAKE_TOOLCHAIN_FILE=build/generators/conan_toolchain.cmake -GNinja
cmake --build build --target xrpl.libxrpl
```

## Building Fuzz Targets

```bash
cd src/test/fuzz

# Transaction parsing
clang++ -std=c++20 -fsanitize=fuzzer,address,undefined \
  -I ../../../include \
  FuzzTransactionParsing.cpp \
  -o fuzz_tx_parsing \
  -L../../../build/lib -lxrpl

# Falcon verification (requires -Dliboqs=ON build)
clang++ -std=c++20 -fsanitize=fuzzer,address,undefined \
  -I ../../../include \
  FuzzFalconVerify.cpp \
  -o fuzz_falcon_verify \
  -L../../../build/lib -lxrpl -loqs

# Fee-split formula
clang++ -std=c++20 -fsanitize=fuzzer,address,undefined \
  -I ../../../include \
  FuzzFeeSplit.cpp \
  -o fuzz_fee_split \
  -L../../../build/lib -lxrpl
```

## Running

```bash
mkdir -p corpus/tx corpus/falcon corpus/feesplit

./fuzz_tx_parsing   corpus/tx/      -max_len=4096  -timeout=30 -runs=1000000
./fuzz_falcon_verify corpus/falcon/ -max_len=8192  -timeout=30 -runs=1000000
./fuzz_fee_split    corpus/feesplit/ -max_len=64   -timeout=10 -runs=10000000
```

## CI Integration

For CI add a short smoke run (10 000 iterations) gating on exit code 0:

```yaml
- name: Fuzz smoke
  run: |
    ./fuzz_tx_parsing    -runs=10000 -timeout=5
    ./fuzz_fee_split     -runs=10000 -timeout=5
```
