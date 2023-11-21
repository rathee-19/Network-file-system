FLAGS = -Werror -Wall -g3
COMMON = src/common/*c src/common/wrappers/*c

main: src/common/* src/client/* src/server/* src/storage/* 
	gcc $(FLAGS) $(COMMON) src/client/*.c -o client
	gcc $(FLAGS) $(COMMON) src/server/*.c -o server
	gcc $(FLAGS) $(COMMON) src/storage/*.c -o storage

log: src/common/* src/client/* src/server/* src/storage/* 
	gcc -DLOG $(FLAGS) $(COMMON) src/client/*.c -o client
	gcc -DLOG $(FLAGS) $(COMMON) src/server/*.c -o server
	gcc -DLOG $(FLAGS) $(COMMON) src/storage/*.c -o storage

debug: src/common/* src/client/* src/server/* src/storage/* 
	gcc -DDEBUG $(FLAGS) $(COMMON) src/client/*.c -o client
	gcc -DDEBUG $(FLAGS) $(COMMON) src/server/*.c -o server
	gcc -DDEBUG $(FLAGS) $(COMMON) src/storage/*.c -o storage

ldebug: src/common/* src/client/* src/server/* src/storage/* 
	gcc -DLOG -DDEBUG $(FLAGS) $(COMMON) src/client/*.c -o client
	gcc -DLOG -DDEBUG $(FLAGS) $(COMMON) src/server/*.c -o server
	gcc -DLOG -DDEBUG $(FLAGS) $(COMMON) src/storage/*.c -o storage

clean:
	rm -f client server storage
