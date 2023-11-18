#include "defs.h"

void init_cache(cache_t* cache) {
    cache->latest=0;
    cache->available=0;
    for(int i=0;i<NUM_CACHED;i++) {
        cache->storage_servers[i]=NULL;
        cache->paths[i]=(char*)malloc(PATH_MAX*sizeof(char));
        for(int j=0;j<PATH_MAX;j++) {
            cache->paths[i][j]='\0';
        }
    }
}

void cache_path(cache_t* cache, storage_t* ss, char* path) {
    cache->storage_servers[cache->available]=ss;

    if(strlen(path)>=PATH_MAX) {
        perror_tx("Path too long");
    }

    strcpy(cache->paths[cache->available], path);
    
    cache->available=(cache->available+1)%NUM_CACHED;
}

storage_t* cache_retrieve(cache_t* cache, char* path) {
    int found=0;

    for(int i=0;i<NUM_CACHED;i++) {
        if(strcmp(cache->paths[i], path)==0) {
            found=1;

            int current_position=i;
            while(current_position==(cache->available-1+NUM_CACHED)%NUM_CACHED) {
                char* selected=cache->paths[current_position];
                cache->paths[current_position]=cache->paths[(current_position+1)%NUM_CACHED];
                cache->paths[(current_position+1)%NUM_CACHED]=selected;

                storage_t* selectedss=cache->storage_servers[current_position];
                cache->storage_servers[current_position]=cache->storage_servers[(current_position+1)%NUM_CACHED];
                cache->storage_servers[(current_position+1)%NUM_CACHED]=selectedss;

                current_position=(current_position+1)%NUM_CACHED;
            }

            break;
        }
    }

    if(found==1) {
        return cache->storage_servers[(cache->available-1+NUM_CACHED)%NUM_CACHED];
    }
    else {
        return NULL;
    }
}