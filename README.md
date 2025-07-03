# FastKey - Redis Clone

## Project Overview
FastKey is a Redis clone implementation written in C that supports core Redis functionality including key-value storage, replication, streams, transactions, and concurrent client handling.

### Key Features
- **Redis Protocol Compatibility**: Implements Redis Serialization Protocol (RESP)
- **Core Commands**: SET, GET, DEL, PING, ECHO, CONFIG, INFO, KEYS, TYPE, XADD, XRANGE, XREAD
- **Replication**: Master-slave replication with PSYNC, REPLCONF commands
- **Transactions**: MULTI, EXEC, DISCARD support
- **Streams**: Redis streams with XADD, XRANGE, XREAD operations
- **Concurrency**: Thread pool for handling multiple client connections
- **Persistence**: RDB file format support
- **Expiration**: TTL support for keys
- **Logging**: Comprehensive logging system

## Getting Started
### Prerequisites
- **C Compiler**: GCC or compatible C compiler
- **POSIX Threads**: pthread library for threading support
- **Standard C Libraries**: stdlib, string, time, etc.

### Building
```bash
# Clone the repository
git clone https://github.com/botirk38/fastkey.git
cd fastkey

# Build using the provided script
./your_program.sh

# Or build manually
gcc -o fastkey app/*.c -lpthread
```

### Running
```bash
# Start the server (default port 6379)
./fastkey

# Start with custom configuration
./fastkey --port 8080 --dir /tmp --dbfilename dump.rdb

# Start as replica
./fastkey --replicaof localhost 6379
```

## Usage
### Connecting
Connect using any Redis client (redis-cli, telnet, etc.):
```bash
redis-cli -p 6379
```

### Supported Commands
- **String Operations**: SET, GET, DEL
- **Server**: PING, ECHO, CONFIG GET, INFO
- **Keys**: KEYS, TYPE, EXPIRE
- **Replication**: PSYNC, REPLCONF
- **Transactions**: MULTI, EXEC, DISCARD
- **Streams**: XADD, XRANGE, XREAD
- **Blocking**: WAIT (master-replica synchronization)

## Architecture
### Core Components
- **Server**: Main server loop and connection handling
- **Client Handler**: Per-client connection management
- **Command Parser**: RESP protocol parsing and command execution
- **Redis Store**: In-memory key-value storage with thread safety
- **Replication**: Master-slave replication logic
- **Streams**: Redis streams implementation
- **Thread Pool**: Concurrent client handling
- **Logger**: Structured logging system

### Thread Safety
The implementation uses read-write locks to ensure thread-safe access to the shared data store while allowing concurrent reads.

## Configuration
### Command Line Options
- `--port`: Server port (default: 6379)
- `--dir`: Working directory for RDB files
- `--dbfilename`: RDB filename
- `--replicaof`: Configure as replica of specified master

### Environment
Logging level can be configured via the logger initialization in main.c.

## Development
### Testing
This project is designed to work with CodeCrafters Redis challenges:
```bash
# Run CodeCrafters tests
codecrafter test
```

### Code Quality
The codebase follows these principles:
- Memory safety with proper cleanup
- Thread safety for concurrent access
- Error handling and logging
- Modular design with clear separation of concerns

## License
This project is for educational purposes as part of the CodeCrafters Redis implementation challenge.
