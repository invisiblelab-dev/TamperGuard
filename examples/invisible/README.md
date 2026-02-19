# Invisible Example

This directory contains an example, named Invisible, that demonstrates how to interact with the Invisible Storage C bindings.

## Overview
The `Invisible` example showcases how to:

- Interact with **S3** via [OpenDAL](https://opendal.apache.org/)
- Interact with the **Solana** blockchain using the Solana SDK

## Files Included

- `invisible.c` – Main source file with logic for interacting with Invisible Storage
- `invisible.h` – Header file
- `Makefile` – Automates the setup process, including:
  - Cloning the Invisible Storage C bindings repository (release [0.0.1](https://github.com/invisiblelab-dev/invisible-storage-bindings/releases/tag/0.0.1)).
  - Compiling the example and linking against the necessary libraries for S3 and Solana operations.

## Requirements

- Rust
- Configure a Solana devnet using Solana CLI
- Configure an S3 bucket

## Configuration

- follow the config instructions provided in the [Invisible Storage](https://github.com/invisiblelab-dev/invisible-storage?tab=readme-ov-file#config) repository.

## Running the Example

```bash
make
./invisible
```
