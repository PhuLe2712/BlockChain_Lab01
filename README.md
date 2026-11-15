# Lab 01 - Blockchain Layer 1 Simulator (C++20)

## Current milestone
This repository contains the foundational modules: Ed25519 identity/signatures, SHA-256, signed transactions, deterministic key-value state execution, block-header signing, and deterministic file logging. It is the first implementation milestone; the P2P simulator and BFT consensus engine are not included yet.

## Requirements (Windows 11)
- Visual Studio 2022 with **Desktop development with C++** workload
- CMake 3.20+
- Git (optional)
- OpenSSL 3.x development package

## Install OpenSSL with vcpkg
```powershell
git clone https://github.com/microsoft/vcpkg C:\dev\vcpkg
C:\dev\vcpkg\bootstrap-vcpkg.bat
C:\dev\vcpkg\vcpkg.exe install openssl:x64-windows
```

## Configure and build
Run from the project root in **Developer PowerShell for VS 2022**:
```powershell
cmake -S . -B build -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

## Run demo and tests
```powershell
.\build\Release\lab01.exe
ctest --test-dir build -C Release --output-on-failure
```
The demo writes `logs/run.log`. Fixed demo seeds are used only to make this simulator reproducible; they are not suitable for production key generation.

## Design notes
- Transaction signatures use the `TX:lab01` domain.
- Block-header signatures use the `HEADER:lab01` domain.
- Transaction keys must be inside the sender namespace (`<public-key-hex>/...`).
- Nonces start at zero and increase by one per sender; duplicate/replayed transactions are rejected.
- State hashing uses sorted `std::map` iteration and length-prefixed strings for deterministic encoding.
