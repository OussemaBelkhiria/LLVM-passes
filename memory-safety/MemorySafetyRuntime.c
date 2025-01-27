#pragma clang diagnostic push
#pragma ide diagnostic ignored "bugprone-reserved-identifier"
/// The code in this file is just a skeleton. You are allowed (and encouraged!)
/// to change if it doesn't fit your needs or ideas.

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
unsigned char* shadow_memory = NULL;
unsigned char* user_memory = NULL;
typedef struct MemoryList {
    bool isfreed;
    unsigned char* address;
    unsigned char* left_rz_start;
    unsigned char* right_rz_start;
    size_t size;

    struct MemoryList* next;
}MemoryList;
struct MemoryList* blocks = NULL;

__attribute__((used))
extern void __runtime_init() {
    
}

__attribute__((used))
extern void __runtime_cleanup() {
    //fprintf(stderr,"program finished, cleaning\n");
    struct MemoryList* curr = blocks;
    struct MemoryList* elem = NULL;
    while (curr) {
        elem = curr->next;
        free(curr);
        curr = elem;
    }
}

__attribute__((used))
extern void* __runtime_stack(void* stackaddr, size_t size) {
         
         unsigned char* casted_addr = (unsigned char*) stackaddr;
        
         struct MemoryList* new_block = (struct MemoryList*) malloc(sizeof(struct MemoryList));
         size_t size_without_padding = size - 32;
         new_block->address = casted_addr + 16;

         //fprintf(stderr,"stack is called with %p and size %lu and the new starting addr is %p\n",casted_addr,size,new_block->address);
         new_block->left_rz_start = (unsigned char*) casted_addr;
        
         //fprintf(stderr,"left zone start is %p \n", new_block->left_rz_start);
         new_block->right_rz_start = new_block->address + size_without_padding;
         new_block->size = size_without_padding;
         new_block->isfreed = false;
         //fprintf(stderr,"left zone start 2 is %p\n\n", new_block->left_rz_start);
         if (blocks == NULL) {
             blocks = new_block;
         }
         else {
            new_block->next = blocks;
            blocks = new_block;
         }
         return new_block->address;

}

__attribute__((used))
extern void __runtime_check_addr(void* addr) {
 
      unsigned char* casted_addr = (unsigned char*) addr;
      //fprintf(stderr,"check is called on : %p\n", casted_addr);
      // check in which block is casted_addr ;
      struct MemoryList* curr = blocks;
      while (curr) {
          // left red zone : 
          if ((casted_addr >= curr->left_rz_start && casted_addr < curr->address)) {
          //      fprintf(stderr,"Blockaddress %p , and size %lu found in red zone : [ %p, %p] \n",curr->address,curr->size,curr->left_rz_start, curr->address);
                fprintf(stderr,"underflow detected\n");
                fprintf(stderr,"Illegal memory access\n");
                __runtime_cleanup();
                exit(1);
          }
          //right red zone :
         else if ((casted_addr >= curr->right_rz_start && casted_addr < (curr->right_rz_start + 16))) 
                {
            //fprintf(stderr,"Blockaddress %p , and size %lu found in right zone : [ %p, %p] \n",curr->address,curr->size,curr->right_rz_start, curr->right_rz_start + 16);
                    fprintf(stderr,"overflow detected\n");
                    fprintf(stderr,"Illegal memory access\n");
                    __runtime_cleanup();
                    exit(2);
                }
          // use after free 
          else if ((casted_addr >= curr->address && casted_addr < (curr->right_rz_start) && curr->isfreed)) {
                    fprintf(stderr,"use after free detected\n");
                    fprintf(stderr,"Illegal memory access\n");
                    __runtime_cleanup();
                    exit(3);

          }
          curr = curr->next;   
      }
}

__attribute__((used))
extern void *__runtime_malloc(size_t size) {
    // adding the left and right red zone ;
    //fprintf(stderr,"malloc is called\n");
    
    void* block = malloc(size + 32);
    unsigned char* block_start = (unsigned char*) block + 16;

    struct MemoryList* new_block = (struct MemoryList*) malloc(sizeof(struct MemoryList));
    new_block->address = block_start;
    new_block->left_rz_start = (unsigned char*) block;
    new_block->right_rz_start = block_start + size;
    new_block->size = size;
    
    //fprintf(stderr,"Block allocated :at address %p with size %lu : and starting left_rz %p and starting right_rz %p\n",new_block->address,size,new_block->left_rz_start,new_block->right_rz_start);
    
    if (blocks == NULL) {
        blocks = new_block;
    }
    else { new_block->next = blocks;
           blocks = new_block;
    }
    return block_start;
}

__attribute__((used))
extern void __runtime_free(void *ptr) {
    // check if ptr is at the beginning of a block : 
    unsigned char* ptr_tofree = (unsigned char*) ptr; 
    //fprintf(stderr,"free is called on %p\n", ptr_tofree);   
    struct MemoryList* curr = blocks;
    struct MemoryList* prev = NULL;
    while(curr) {
        if (curr->address == ptr_tofree) {
            // free the original pointer that malloc returned : block + red zones
            free(curr->left_rz_start);
            curr->isfreed = true;
            return;
        }
        curr = curr->next;
    }
   
}

#pragma clang diagnostic pop
