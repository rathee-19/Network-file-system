# Network File System

## Some pointers

1. The program makes use of wrapper functions extensively, for a variety of reasons as listed below, to streamline the code of operational segments.

    - Retrying error prone functions indefinitely, with a timestamp (function_t).
    - Retrying error prone functions, with a bound on attempts (function_b).
    - Dealing with errors but with an exit (function_x).
    - Dealing with errors but with a thread-specific shutdown hook (function_p, as in pthread).

2. There is no way the naming server can differentiate between the clients and storage servers yet, except for the kind of messages they send ofcourse. The easiest thing that can be done is to open two ports for each, but I am not sure how far that would go.

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
