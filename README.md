# Network File System

## Storage Server

1. Every storage server is uniquely identified by its port number dedicated to the naming server.

2. The server searches for a directory with the same identifier, using it as the root for all its operations.

3. For the client port, we just increment the identifier by 1.

4. We also maintain a special port for copying files among the storage servers, obtained by incrementing the identifier by 2.