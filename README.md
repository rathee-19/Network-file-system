# Network File System

## Steps to run

1. Run in default mode.

```sh
make
./server
./storage <nsport> <clport> <stport>
./client
```

2. Run in debug mode.

```sh
make debug
...
```

3. Run in logging mode.

```sh
make log
./server <logfile>
./storage <nsport> <clport> <stport> <logfile>
./client <logfile>
```

4. Run in both logging and debug mode.

```sh
make ldebug
...
```

Note: The server, by definition, uses ``NSPORT 7001``. Be considerate of that when initialising the storage servers.
