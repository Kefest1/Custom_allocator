//
// Created by root on 12/8/22.
//

#include "heap.h"
#include "tested_declarations.h"
#include "tested_declarations.h"
#include "rdebug.h"
#include "tested_declarations.h"
#include "rdebug.h"


int heap_setup(void) {

    void *tempPtr = custom_sbrk((intptr_t) 0);

    if (tempPtr == CUSTOM_SBRK_ERROR_CODE)
        return HEAP_SETUP_ERR_CODE;

    structMemoryController.first_memory_chunk = NULL;
    structMemoryController.memory_start = tempPtr;

    return HEAP_SETUP_OK;
}

void heap_clean(void) {
    void *temp = (char *) custom_sbrk((intptr_t) 0);


    if (temp == CUSTOM_SBRK_ERROR_CODE)
        return;

    intptr_t totalAllocatedMemory = (char *) custom_sbrk((intptr_t) 0) - (char *) structMemoryController.memory_start;

    if (custom_sbrk(-1 * totalAllocatedMemory) == CUSTOM_SBRK_ERROR_CODE)
        return;

    structMemoryController.first_memory_chunk = structMemoryController.memory_start = NULL;
}


size_t heap_get_largest_used_block_size(void) {
    size_t max = 0;

    if (heap_validate() != HEAP_VALID)
        return 0;

    struct memory_chunk_t *buffer = structMemoryController.first_memory_chunk;

    while (buffer) {

        if (buffer->free != FREE)
            max = buffer->size > max ? buffer->size : max;

        buffer = buffer->next;
    }

    return max;
}


void *addFences(void *toFencePointer, size_t blockSize) {
    char *buffer = toFencePointer;

    for (int i = 0; i < FENCE_SIZE; i++)
        *(buffer + i) = FENCE_CHAR;

    for (int i = 0; i < FENCE_SIZE; i++)
        *(buffer + (size_t) FENCE_SIZE + blockSize + i) = FENCE_CHAR;

    return toFencePointer;
}

#define IS_EMPTY structMemoryController.first_memory_chunk == NULL

void* heap_malloc(size_t size) {
    if (heap_validate() != HEAP_VALID || size == 0)
        return NULL;

    void *tempPointer;

    if (IS_EMPTY) {
        tempPointer = custom_sbrk((intptr_t) (size + CHUNK_SIZE + FENCE_SIZE + FENCE_SIZE));

        if (tempPointer == CUSTOM_SBRK_ERROR_CODE)
            return NULL;

        structMemoryController.first_memory_chunk = structMemoryController.memory_start;

        structMemoryController.first_memory_chunk->next = structMemoryController.first_memory_chunk->prev = NULL;
        structMemoryController.first_memory_chunk->free = OCCUPIED;
        structMemoryController.first_memory_chunk->size = size;


        return (void *) ((char *) addFences( ((char *) structMemoryController.first_memory_chunk + CHUNK_SIZE), size) + FENCE_SIZE);
    }

    struct memory_chunk_t *chunkIterator = structMemoryController.first_memory_chunk;
    struct memory_chunk_t *buffer;



    while (chunkIterator) {
        if (chunkIterator->free == FREE && chunkIterator->size >= size + DOUBLE_FENCE_SIZE) {
            chunkIterator->size = size;
            chunkIterator->free = OCCUPIED;

            return (void *) ((char *)addFences((void *) ((uint8_t *) chunkIterator + CHUNK_SIZE), size) + FENCE_SIZE);
        }
        buffer = chunkIterator;
        chunkIterator = chunkIterator->next;
    }


    tempPointer = custom_sbrk((intptr_t) (size + CHUNK_SIZE + DOUBLE_FENCE_SIZE));
    if (tempPointer == CUSTOM_SBRK_ERROR_CODE)
        return NULL;


    struct memory_chunk_t *lastElement = (struct memory_chunk_t *) ((uint8_t *) buffer + buffer->size + CHUNK_SIZE + DOUBLE_FENCE_SIZE);


    lastElement->next = NULL;
    lastElement->prev = buffer;

    lastElement->size = size;
    lastElement->free = OCCUPIED;

    buffer->next = lastElement;

    return (void *) ((char *) addFences((void *) ((uint8_t *) lastElement + CHUNK_SIZE), size) + FENCE_SIZE);
}


void* heap_calloc(size_t number, size_t size) {
    size_t blockSize = number * size;
    char *toReturn = (char *) heap_malloc(blockSize);

    if (!toReturn) return NULL;

    for (size_t i = 0; i < blockSize; i++)
        *(toReturn + i) = CALLOC_FILL;

    return toReturn;
}

void* heap_realloc(void* memblock, size_t size) {
    if (heap_validate() != HEAP_VALID)
        return NULL;

    if (!memblock)
        return heap_malloc(size);

    if (!size)
        return heap_free(memblock), NULL;

    if (get_pointer_type(memblock) != pointer_valid)
        return NULL;

    struct memory_chunk_t *currentMemoryChunk =
            (struct memory_chunk_t *) ((char *) memblock - FENCE_SIZE - CHUNK_SIZE);

    size_t currentChunkSize = currentMemoryChunk->size;

    if (currentMemoryChunk->next != NULL)
        currentChunkSize = (char *) currentMemoryChunk->next - (char *) currentMemoryChunk - CHUNK_SIZE - FENCE_SIZE - FENCE_SIZE;

    if (size < currentMemoryChunk->size) {
        if (currentMemoryChunk->next == NULL)
            custom_sbrk((intptr_t) (size - currentMemoryChunk->size));

        addFences((char *) currentMemoryChunk + CHUNK_SIZE, size);

        return currentMemoryChunk->size = size, memblock;
    }

    if (currentMemoryChunk->size == size)
        return memblock;

    if (currentMemoryChunk->next == NULL) {
        void *testSbrk = custom_sbrk((intptr_t) (size - currentMemoryChunk->size));
        if (testSbrk == CUSTOM_SBRK_ERROR_CODE)
            return NULL;

        currentMemoryChunk->size = size;
        addFences((char *) currentMemoryChunk + CHUNK_SIZE, size);

        return memblock;
    }

    if (currentMemoryChunk->next->free == FREE) {

        if (currentMemoryChunk->next->size >= size) {
            struct memory_chunk_t *pChunk = (struct memory_chunk_t *) ((uint8_t *) currentMemoryChunk->next + size);

            for (size_t i = 0; i < CHUNK_SIZE; i++)
                *((char *) pChunk + i) = *((char *) currentMemoryChunk->next + i);

            currentMemoryChunk->next = pChunk;
            pChunk->next->prev = pChunk;
            pChunk->size -= size;

            currentMemoryChunk->size = size;
            addFences((uint8_t *) currentMemoryChunk + CHUNK_SIZE, size);

            return memblock;
        }

        if ((size_t) ((char *) currentMemoryChunk->next + currentMemoryChunk->next->size - (char *) currentMemoryChunk - DOUBLE_FENCE_SIZE) >= size) {
            currentMemoryChunk->next = currentMemoryChunk->next->next;
            currentMemoryChunk->next->prev = currentMemoryChunk;

            currentMemoryChunk->size = size;

            addFences((char *) currentMemoryChunk + CHUNK_SIZE, size);

            return memblock;
        }

        if (currentMemoryChunk->next->size + currentChunkSize + DOUBLE_FENCE_SIZE * CHUNK_SIZE >= size) {

            struct memory_chunk_t *tempStruct = (struct memory_chunk_t *) ((uint8_t *) currentMemoryChunk->next + size -
                                                                           currentChunkSize);

            for (size_t i = 0; i < CHUNK_SIZE; i++)
                *((char *) tempStruct + i) = *((char *) currentMemoryChunk->next + i);


            tempStruct->size = tempStruct->size - (size - currentChunkSize);
            currentMemoryChunk->next = tempStruct;
            currentMemoryChunk->next->next->prev = tempStruct;

            currentMemoryChunk->size = size;
            addFences((char *) currentMemoryChunk + CHUNK_SIZE, size);

            return memblock;
        }
    }

    struct memory_chunk_t *newMemory = heap_malloc(size);

    if (newMemory == NULL)
        return NULL;


    for (size_t i = 0; i < currentMemoryChunk->size; i++)
        *((char *) newMemory + i) = *((char *) memblock + i);

    heap_free((void *) memblock);

    return newMemory;
}



void heap_free(void* memblock) {
    if (heap_validate() != HEAP_VALID || !memblock)
        return;

    struct memory_chunk_t *chunkToFree = structMemoryController.first_memory_chunk;

    while (chunkToFree != (struct memory_chunk_t *) ((char *) memblock - FENCE_SIZE - CHUNK_SIZE)) {
        if (!chunkToFree)
            return;

        chunkToFree = chunkToFree->next;
    }

    if (chunkToFree->free == FREE)
        return;
    else chunkToFree->free = FREE;

    if (chunkToFree->next && chunkToFree->next->free == FREE) {
        chunkToFree->size += chunkToFree->next->size + CHUNK_SIZE + DOUBLE_FENCE_SIZE;

        if (chunkToFree->next->next != NULL)
            chunkToFree->next->next->prev = chunkToFree;

        chunkToFree->next = chunkToFree->next->next;
    }

    if (chunkToFree->prev && chunkToFree->prev->free == FREE) {
        chunkToFree->prev->size += chunkToFree->size + CHUNK_SIZE + DOUBLE_FENCE_SIZE;
        chunkToFree->prev->next = chunkToFree->next;

        if (chunkToFree->next != NULL)
            chunkToFree->next->prev = chunkToFree->prev;


        chunkToFree = chunkToFree->prev;
    }


    if (!chunkToFree->prev && !chunkToFree->next) {
        char* temp = custom_sbrk((intptr_t) 0);

        structMemoryController.first_memory_chunk = NULL;
        custom_sbrk(((char *) structMemoryController.memory_start - temp));
    }
    else
        chunkToFree->size = (uint8_t *) chunkToFree->next - (uint8_t *) chunkToFree - CHUNK_SIZE;


    if (!chunkToFree->next && chunkToFree->prev != NULL) {
        chunkToFree->prev->next = NULL;

        custom_sbrk((intptr_t) (-1 * (((uint8_t *) custom_sbrk((intptr_t) 0)) - (uint8_t *) chunkToFree->prev - DOUBLE_FENCE_SIZE - CHUNK_SIZE - chunkToFree->prev->size)));
    }

}



enum pointer_type_t get_pointer_type(const void* const pointer) {
    if (!pointer)
        return pointer_null;

    if (heap_validate() != HEAP_VALID)
        return pointer_heap_corrupted;

    struct memory_chunk_t *buffer;

    uint8_t *tempHelpPointer = (uint8_t *) pointer;

    for (buffer = structMemoryController.first_memory_chunk; buffer != NULL; buffer = buffer->next) {
        uint8_t *chunkStart = (uint8_t *) buffer;
        uint8_t *chunkEnd = (uint8_t *) buffer + buffer->size + CHUNK_SIZE;


        if (buffer->free == FREE)
            if (chunkEnd >= tempHelpPointer && tempHelpPointer >= chunkStart + CHUNK_SIZE)
                return pointer_unallocated;

        if (tempHelpPointer >= chunkStart) {
            if (tempHelpPointer < chunkStart + CHUNK_SIZE)
                return pointer_control_block;

            else if (tempHelpPointer == chunkStart + FENCE_SIZE + CHUNK_SIZE)
                return pointer_valid;

            else if (tempHelpPointer >= chunkStart + CHUNK_SIZE && tempHelpPointer < chunkStart + FENCE_SIZE + CHUNK_SIZE)
                return pointer_inside_fences;

            else if (tempHelpPointer >= chunkEnd + FENCE_SIZE && tempHelpPointer < chunkEnd + FENCE_SIZE + FENCE_SIZE)
                return pointer_inside_fences;

            else if (tempHelpPointer > chunkStart + FENCE_SIZE + CHUNK_SIZE && tempHelpPointer <= chunkEnd + FENCE_SIZE)
                return pointer_inside_data_block;

        }

        if (tempHelpPointer > (uint8_t *) custom_sbrk((intptr_t) 0))
            return pointer_unallocated;
    }

    return pointer_unallocated;
}


int heap_validate(void) {
    if (structMemoryController.memory_start == NULL)
        return HEAP_UNINITIALISED;

    struct memory_chunk_t *buffer = structMemoryController.first_memory_chunk;
    struct memory_chunk_t *endOfMemory = custom_sbrk((intptr_t) 0);


    while (buffer != NULL && buffer->next != NULL) {

        if (FREE_FIELD_VALIDATE(buffer->free))
            return HEAP_DAMAGED_CONTROL;

        if (buffer->free == FREE && buffer->size + CHUNK_SIZE > (unsigned long) ((uint8_t *) buffer->next - (uint8_t *) buffer)) {
            return HEAP_DAMAGED_CONTROL;
        }

        if (buffer->free == OCCUPIED && buffer->size == 0) {
            return HEAP_DAMAGED_CONTROL;
        }

        if (buffer->next != NULL) {

            if (buffer >= buffer->next || buffer->next >= endOfMemory)
                return HEAP_DAMAGED_CONTROL;

            if (buffer->next->prev != buffer)
                return HEAP_DAMAGED_CONTROL;

            buffer = buffer->next;
        }
    }

    if (buffer) {
        if ((uint8_t *) buffer + buffer->size + CHUNK_SIZE + FENCE_SIZE + FENCE_SIZE > (uint8_t *) endOfMemory)
            return HEAP_DAMAGED_CONTROL;

        if (buffer->free != 0)
            return HEAP_DAMAGED_CONTROL;
    }


    return checkFences();
}

int checkFences(void) {
    struct memory_chunk_t *buffer = structMemoryController.first_memory_chunk;

    while (buffer) {
        if (buffer->free == OCCUPIED) {

            int j;
            uint8_t *helpBuf = (uint8_t *) buffer + CHUNK_SIZE;

            for (j = 0; j < FENCE_SIZE; j++) {
                if (*(helpBuf + j) != FENCE_CHAR)
                    return HEAP_DAMAGED_FENCES;
                if (*(helpBuf + j + buffer->size + FENCE_SIZE) != FENCE_CHAR)
                    return HEAP_DAMAGED_FENCES;
            }
        }

        buffer = buffer->next;
    }

    return HEAP_VALID;
}


