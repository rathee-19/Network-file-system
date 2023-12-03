FLAGS   := -Werror -Wall -g3
COMMON  := src/common/*c src/common/wrappers/*c
TERM    := kgx --tab
EXECDIR := /home/himanshu/nfs
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
	cp $(EXECDIR)/storage $(ROOTDIR)/dir_pbcui
	cp $(EXECDIR)/storage $(ROOTDIR)/dir_gywnw/dir_fzxpq
	cp $(EXECDIR)/storage $(ROOTDIR)/dir_vhfih/dir_lhkzx
	cp $(EXECDIR)/storage $(ROOTDIR)/dir_psjio/dir_ctyuy
	cp $(EXECDIR)/storage $(ROOTDIR)/dir_psjio/dir_kgabk/dir_jtngu
	cp $(EXECDIR)/storage $(ROOTDIR)/dir_vymdo
	cp $(EXECDIR)/storage $(ROOTDIR)/a_song_ice_fire
	cp $(EXECDIR)/storage $(ROOTDIR)/gravity_falls
	cp $(EXECDIR)/storage $(ROOTDIR)/dir_gywnw/dir_fzxpq/dir_wewny

	$(TERM) --title="CLIENT" --working-directory=$(EXECDIR) --command="./client"
	$(TERM) --title="SERVER" --working-directory=$(EXECDIR) --command="./server"
	$(TERM) --title="SS1"    --working-directory=$(ROOTDIR)/dir_pbcui --command="./storage 8001 8002 8003 < $(EXECDIR)/input/input1"
	$(TERM) --title="SS2"    --working-directory=$(ROOTDIR)/dir_gywnw/dir_fzxpq --command="./storage 8004 8005 8006 < $(EXECDIR)/input/input2"
	$(TERM) --title="SS3"    --working-directory=$(ROOTDIR)/dir_vhfih/dir_lhkzx --command="./storage 8007 8008 8009 < $(EXECDIR)/input/input3"
	$(TERM) --title="SS4"    --working-directory=$(ROOTDIR)/dir_psjio/dir_ctyuy --command="./storage 8010 8011 8012 < $(EXECDIR)/input/input4"
	$(TERM) --title="SS5"    --working-directory=$(ROOTDIR)/dir_psjio/dir_kgabk/dir_jtngu --command="./storage 8013 8014 8015 < $(EXECDIR)/input/input5"
	$(TERM) --title="SS6"    --working-directory=$(ROOTDIR)/dir_vymdo --command="./storage 8016 8017 8018 < $(EXECDIR)/input/input6"
	$(TERM) --title="SS7"    --working-directory=$(ROOTDIR)/a_song_ice_fire --command="./storage 8019 8020 8021 < $(EXECDIR)/input/input7"
	$(TERM) --title="SS8"    --working-directory=$(ROOTDIR)/gravity_falls --command="./storage 8022 8023 8024 < $(EXECDIR)/input/input8"
	$(TERM) --title="SS9"    --working-directory=$(ROOTDIR)/dir_gywnw/dir_fzxpq/dir_wewny --command="./storage 8025 8026 8027 < $(EXECDIR)/input/input9"

clean:
	rm -f client server storage
