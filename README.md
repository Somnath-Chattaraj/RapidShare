# RapidShare

RapidShare is a small C++17 client-server file transfer project. It sends files over TCP, continues interrupted uploads, verifies file integrity, and handles several clients at the same time.

## Features

- TCP file uploads
- resumable partial transfers
- FNV-1a 64-bit integrity check
- background file reader with a bounded thread-safe queue
- one server worker thread per client
- configurable transfer chunk size
- progress and throughput display
- filename sanitization
- no third-party dependencies

## Build

Requirements: a C++17 compiler and macOS or Linux.

Using Make:

```bash
make
make test
```

Using CMake 3.16+:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Run

Start the server:

```bash
./build/rapidshare_server 9000 received_files
```

Upload a file from another terminal:

```bash
./build/rapidshare_client 127.0.0.1 9000 path/to/file.zip
```

The optional fourth client argument changes the chunk size in KB:

```bash
./build/rapidshare_client 127.0.0.1 9000 path/to/file.zip 512
```

If a connection ends early, run the same client command again. The server keeps the `.part` file and tells the client where to continue.

## Design

The client sends metadata followed by length-prefixed data chunks. A background thread reads the source file into a bounded queue while the main thread sends those chunks. The server starts a detached worker for every connection, so separate clients can upload concurrently.

All integers in the wire protocol use network byte order. After the last chunk, the server calculates the checksum of the received file and only moves it to its final name when the checksum and size match.
