# Network File System (NFS) Project

Welcome to the Network File System (NFS) project! This project focuses on implementing a network-based file system inspired by NFS. The primary objective is to understand concurrency and networking concepts and apply them to build a moderately complex system.

## Key Features

- **Concurrency Management**: Efficient handling of multiple client requests simultaneously.
- **Network Communication**: Seamless data transfer between clients and servers over the network.
- **Scalability**: Easily adaptable to handle increasing data and client requests.
- **Fault Tolerance**: Ensures data consistency and availability even in case of errors or failures.


## Running the Code

To run the code, follow these steps in different terminals:

### Naming Server

```bash
bashCopy code
make naming_server
./naming_server

```

### Storage Server

1. Compile the storage server:
    
    ```bash
    bashCopy code
    make storage_server
    
    ```
    
2. Place the executable in the desired directory and run:
    
    ```bash
    bashCopy code
    ./storage_server
    
    ```
    
3. Enter the paths to be accessible to the client and the naming server's IP address in the terminal. The storage server will be ready to accept requests.

### Client

1. Compile the client:
    
    ```bash
    bashCopy code
    make client
    
    ```
    
2. Run the client and enter the naming server's IP address. You can now access files stored on the storage servers through the naming server.

### Finding the IP Address

To find the IP address of the naming server, run:

```bash
bashCopy code
hostname -I

```

## File Descriptions

### Naming Server

- `main.c`: Entry point for the naming server.
- `server_setup.h`: Functions for setting up the server, sending, and receiving messages.
- `hashmap.h`: Implements hashing for storing file paths and storage server details.
- `LRUCaching.h`: Implements the LRU caching algorithm.
- `client_handler.h`: Handles client connection requests.
- `SS_handler.h`: Handles storage server connection requests.
- `operation_handler.h`: Contains APIs for client and storage server operations.
- `utils.h`: Logging, macros, global variables, and utility functions.

### Storage Server

- `main.c`: Entry point for the storage server.
- `server_setup.h`: Functions for setting up the server, sending, and receiving messages.
- `get_accessible_paths.h`: Gets paths from the user and sends them to the naming server.
- `client_handler.h`: Handles client and naming server connection requests.
- `operation_handler.h`: Contains in-server operations and APIs.
- `utils.h`: Macros and global variables.

### Client

- `main.c`: Entry point for the client.
- `server_setup.h`: Functions for setting up the server, sending, and receiving messages.
- `operation_handler.h`: Contains APIs for client operations.
- `utils.h`: Macros and global variables.

## Usage Modes

### Default Mode

```bash
bashCopy code
make
./server
./storage <nsport> <clport> <stport>
./client

```

### Debug Mode

```bash
bashCopy code
make debug
./server
./storage <nsport> <clport> <stport>
./client

```

### Logging Mode

```bash
bashCopy code
make log
./server <logfile>
./storage <nsport> <clport> <stport> <logfile>
./client <logfile>

```

### Debug and Logging Mode

```bash
bashCopy code
make ldebug
./server <logfile>
./storage <nsport> <clport> <stport> <logfile>
./client <logfile>

```

(Note: The server uses port 7001 by default for the naming server. Make sure to consider this when setting up storage servers.)

## Developers
- Himanshu Singh
- Archit Narwadkar
- Rohan Rathee