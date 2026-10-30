# Toolchain stage: everything needed to build and test, and nothing else.
FROM ubuntu:24.04 AS toolchain
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential cmake ninja-build git ca-certificates clang clang-format libclang-rt-18-dev \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src

# Test stage: copy the code in, configure, build, run every test.
# Fails the image build if anything fails.
FROM toolchain AS test
ARG CXX_COMPILER=g++
ARG SANITIZE=OFF
ARG BUILD_TYPE=Debug
# AddressSanitizer's "use after return" check makes every function call far slower
# (found by profiling: a 2 second test took over 10 minutes). Turn that one check off.
ENV ASAN_OPTIONS=detect_stack_use_after_return=0 \
    UBSAN_OPTIONS=print_stacktrace=1
COPY . /src
RUN cmake -S . -B build -G Ninja \
      -DCMAKE_CXX_COMPILER=${CXX_COMPILER} \
      -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
      -DFH_SANITIZE=${SANITIZE} \
 && cmake --build build \
 && ctest --test-dir build --output-on-failure

# Format stage: fails if any file differs from clang-format output.
FROM toolchain AS format
COPY . /src
RUN find include src tests -name '*.hpp' -o -name '*.cpp' | xargs clang-format --dry-run --Werror

# Fuzz stage: builds the libFuzzer target with clang and sanitizers, then runs it
# for FUZZ_SECONDS on a few valid seed packets. Fails the build if it finds a crash.
FROM toolchain AS fuzz
ARG FUZZ_SECONDS=30
COPY . /src
RUN cmake -S . -B build -G Ninja \
      -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug \
      -DFH_SANITIZE=ON -DFH_FUZZ=ON \
 && cmake --build build --target fuzz_handler \
 && bash tests/fuzz/make_seeds.sh /corpus \
 && ASAN_OPTIONS=detect_stack_use_after_return=0 \
    ./build/tests/fuzz/fuzz_handler -max_total_time=${FUZZ_SECONDS} -print_final_stats=1 /corpus
