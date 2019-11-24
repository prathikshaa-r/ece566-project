/***
UNIMPORTANT FILE
Thought of using this for eviction, 
but did it efficiently enough using arrays on the heap...
filenames[index] where index is the key=file_id
----------------------------------------------------------
Keep this for reference only
***/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct hash_t{
	// int file_id; // redundant information
	char filename[1024];
} typedef hash_node;

/*
Returns malloced array
Each row and the doubke pointer must be freed
*/
hash_node* init(size_t hash_size){
	hash_node ** hashArray = (hash_node*)malloc(sizeof(hash_node*)*hash_size);
	// Below can be done only when needed on insert?
	for (int i = 0; i < hash_size; ++i){
		hashArray[i] = (hash_node*)malloc(sizeof(hash_node));
	}
}

hash_node* insert(hash_node *new_node){

}

struct evicted_block_t{
	size_t block_offset;
	char filename[1024]
} typedef evicted_block;