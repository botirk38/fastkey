<p align="center">
    <img src="https://raw.githubusercontent.com/PKief/vscode-material-icon-theme/ec559a9f6bfd399b82bb44393651661b08aaf7ba/icons/folder-markdown-open.svg" align="center" width="30%">
</p>
<p align="center"><h1 align="center">FASTKEY</h1></p>
<p align="center">
 <em>A high-performance key-value store built with C, utilizing a thread pool for concurrency and the RESP protocol for communication.</em>
</p>
<p align="center">
 <img src="https://img.shields.io/github/license/botirk38/fastkey?style=default&logo=opensourceinitiative&logoColor=white&color=0080ff" alt="license">
 <img src="https://img.shields.io/github/last-commit/botirk38/fastkey?style=default&logo=git&logoColor=white&color=0080ff" alt="last-commit">
 <img src="https://img.shields.io/github/languages/top/botirk38/fastkey?style=default&color=0080ff" alt="repo-top-language">
 <img src="https://img.shields.io/github/languages/count/botirk38/fastkey?style=default&color=0080ff" alt="repo-language-count">
</p>
<br>

## Table of Contents

- [Overview](#-overview)
- [Features](#-features)
- [Project Structure](#-project-structure)
  - [Project Index](#-project-index)
- [Getting Started](#-getting-started)
  - [Prerequisites](#-prerequisites)
  - [Installation](#-installation)
  - [Usage](#-usage)
  - [Testing](#-testing)
- [Project Roadmap](#-project-roadmap)
- [Contributing](#-contributing)
- [License](#-license)
- [Acknowledgments](#-acknowledgments)

---

## Overview

FASTKEY is a lightweight and efficient key-value store written in C. It leverages a **thread pool** for handling concurrent requests efficiently and implements the **RESP (REdis Serialization Protocol)** for structured communication between clients and the server.

---

## Features

- **High-performance**: Uses a **thread pool** to efficiently manage multiple client requests.
- **RESP Protocol**: Supports the same structured messaging format as Redis for seamless client compatibility.
- **Event-driven architecture**: Implements an **epoll-based event loop** to handle I/O efficiently.
- **Persistence**: Provides an optional **RDB-based** persistence mechanism.
- **Replication support**: Allows for synchronization across multiple instances.

---

## Getting Started

### Prerequisites

Before getting started with FASTKEY, ensure your runtime environment meets the following requirements:

- **Programming Language:** C
- **Build Tools:** GCC/Clang, Make
- **System Support:** Linux (recommended for epoll support)

### Installation

Install FASTKEY using one of the following methods:

**Build from source:**

1. Clone the FASTKEY repository:

```sh
 git clone https://github.com/botirk38/fastkey
```

2. Navigate to the project directory:

```sh
 cd fastkey
```

3. Build the project:

```sh
 make
```

### Usage

Run FASTKEY using the following command:

```sh
 ./fastkey
```

### Testing

Run the test suite using the following command:

```sh
 make test
```

---

## License

This project is protected under the [MIT License](https://choosealicense.com/licenses/mit/). For more details, refer to the [LICENSE](LICENSE) file.

---

## Acknowledgments

- Inspired by Redis architecture.
- Built using **epoll**, **thread pools**, and **RESP protocol** for efficient request handling.

---
