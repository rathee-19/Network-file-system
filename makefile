FLAGS = -Werror -Wall -g3
COMMON = src/common/*c src/common/wrappers/*c

compile: client server storage

client: src/client/* src/common/*
	gcc $(FLAGS) $(COMMON) src/client/*.c -o client

server: src/server/* src/common/*
	gcc $(FLAGS) $(COMMON) src/server/*.c -o server

storage: src/storage/* src/common/*
	gcc $(FLAGS) $(COMMON) src/storage/*.c -o storage

debug: client_debug server_debug storage_debug

client_debug: src/client/* src/common/*
	gcc -DDEBUG $(FLAGS) $(COMMON) src/client/*.c -o client

server_debug: src/server/* src/common/*
	gcc -DDEBUG $(FLAGS) $(COMMON) src/server/*.c -o server

storage_debug: src/storage/* src/common/*
	gcc -DDEBUG $(FLAGS) $(COMMON) src/storage/*.c -o storage

clean:
	rm -f client server storage
