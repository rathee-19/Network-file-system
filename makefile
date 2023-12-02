FLAGS   := -Werror -Wall -g3
COMMON  := src/common/*c src/common/wrappers/*c
EXECDIR := /home/himanshu/nfs
TERM    := kgx --tab --working-directory=$(EXECDIR)
ROOTDIR := /home/himanshu/root_directory

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

test:
	# git clone https://github.com/serc-courses/course-project-dir
	$(TERM) --title="CLIENT" --command="./client"
	$(TERM) --title="SERVER" --command="./server"
	$(TERM) --title="SS1"    --command="echo 1 $(ROOTDIR)/dir_pbcui | ./storage 8001 8002 8003"
	$(TERM) --title="SS2"    --command="echo 1 $(ROOTDIR)/dir_gywnw/dir_fzxpq | ./storage 8004 8005 8006"
	$(TERM) --title="SS3"    --command="echo 1 $(ROOTDIR)/dir_vhfih/dir_lhkzx | ./storage 8007 8008 8009"
	$(TERM) --title="SS4"    --command="echo 1 $(ROOTDIR)/dir_psjio/dir_ctyuy | ./storage 8010 8011 8012"
	$(TERM) --title="SS5"    --command="echo 1 $(ROOTDIR)/dir_psjio/dir_kgabk/dir_jtngu | ./storage 8013 8014 8015"
	$(TERM) --title="SS6"    --command="echo 1 $(ROOTDIR)/dir_vymdo | ./storage 8016 8017 8018"
	$(TERM) --title="SS7"    --command="echo 1 $(ROOTDIR)/a_song_ice_fire | ./storage 8019 8020 8021"
	$(TERM) --title="SS8"    --command="echo 1 $(ROOTDIR)/gravity_falls | ./storage 8022 8023 8024"
	$(TERM) --title="SS9"    --command="echo 1 $(ROOTDIR)/dir_gywnw/dir_fzxpq/dir_wewny | ./storage 8025 8026 8027"

clean:
	rm -f client server storage
