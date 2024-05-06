# FastKey - High-Performance Key-Value Store

**FastKey** is a high-performance key-value store inspired by Redis, designed to deliver enhanced performance and scalability. It supports Redis-compatible commands and extends functionality with features like advanced replication, efficient data streaming, durable RDB persistence, and multithreaded processing.

## Features

- **Redis-Compatible Commands**: Executes a broad range of Redis commands, ensuring compatibility and ease of migration.
- **Replication**: Implements sophisticated replication mechanisms for increased data availability and redundancy.
- **Streams**: Offers full support for Redis streams, enabling high-performance handling of streaming data.
- **RDB Persistence**: Provides robust data persistence through RDB snapshots, ensuring data durability.
- **Multithreaded Server**: Leverages a multithreaded architecture to optimize CPU utilization and performance on multicore systems.

## Installation

### Prerequisites

Ensure your system has the following installed:
- C++ compiler (supporting C++17 or later)
- CMake (version 3.12 or above)
- Make

### Building from Source

To build FastKey, clone the repository and compile as follows:

```bash
# Clone the repository
git clone https://github.com/yourusername/fastkey.git

# Navigate to the project directory
cd fastkey

# Create and enter the build directory
mkdir build && cd build

# Configure the project
cmake ..

# Compile the project
make
```

## Usage

Start FastKey server with default settings using:

```bash
./fastkey
```

### Interacting with FastKey

Use `redis-cli` to interact with FastKey. Here's how you can connect and execute commands:

```bash
# Connect to FastKey
redis-cli -p <port>

# Example command
redis-cli SET key1 "value1"
redis-cli GET key1
```

### Replication Setup

To configure FastKey as a replica, use `redis-cli` to send replication commands:

```bash
# Connect to the replica FastKey instance
redis-cli -p <replica_port>

# Set up replication
REPLCONF listening-port <replica_port>
```

## Example Commands

Here are a few examples illustrating how to use FastKey for various operations:

```bash
# Setting a key
redis-cli SET mykey "Hello, FastKey"

# Retrieving a key
redis-cli GET mykey

# Adding to a stream
redis-cli XADD mystream * sensor-id 1234 temperature 19.7

# Reading from a stream
redis-cli XREAD STREAMS mystream 0-0
```

## Demo

Watch FastKey in action and explore its features through our video demo:

[![Watch the Demo](https://www.loom.com/share/973399db9bae45508d49d0b111e9fb8d?sid=5094886c-c809-4550-93c1-eb6c7beb873b)](https://www.loom.com/share/973399db9bae45508d49d0b111e9fb8d?sid=5094886c-c809-4550-93c1-eb6c7beb873b)

## Contributing

Contributions to FastKey are welcome! Fork the repository, make your enhancements, and submit a pull request. Ensure your contributions adhere to the project's coding standards and include appropriate tests.

## License

FastKey is released under the MIT License. For more details, see the `LICENSE` file in the repository.


