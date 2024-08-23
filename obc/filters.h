#include <stdint.h>
#include <stdbool.h>

// round ( fir1(w, Wn) * 1024 ) 
#define SIZE_4 4
#define SIZE_16 17
#define SIZE_32 33
#define SIZE_128 129

#define IGNORE_SAMPLES 2

typedef struct
{
    int16_t buf[4];
    int16_t head;
    int16_t tail; 
    int16_t wait;
    bool next_OK; 
    int curr_s; // Current sample, increments until buffer size, then curr_s -= 2. Apply filter when curr_s == SIZE_16-1
} filter_4;


typedef struct
{
    int16_t buf[17];
    int16_t head;
    int16_t tail; 
    int16_t wait;
    bool next_OK; 
    int curr_s; // Current sample, increments until buffer size, then curr_s -= 2. Apply filter when curr_s == SIZE_16-1
} filter_16;

typedef struct 
{
    int16_t buf[33];
    int16_t head;
    int16_t tail; 
    int16_t wait;
    bool next_OK; 
    int curr_s; // Current sample, incremetns until buffer size, then curr_s -= 2. Apply filter when curr_s == SIZE_32-1
} filter_32;

typedef struct 
{
    int16_t buf[129];
    int16_t head;
    int16_t tail;
    int16_t wait;
    bool next_OK; 
    int curr_s; // Current sample, increments until buffer size, then curr_s -= 2. Apply filter when curr_s == SIZE_128-1
} filter_128;

int16_t fir_16[17] = {
    0, -4, -12, -17, 0, 55, 140, 221, 255, 221, 140, 55, 0, -17, -12, -4, 0
};

int16_t fir_32[33] = { 
    0, -2, -2, 1, 6, 5, -7, -16, -3, 26, 30, -16, -70, -43, 108, 301, 390, 301, 108, -43, -7, -16 , 30 , 26 , -3, -16, -7, 5, 6, 1, -2, -2, 0
};

int16_t fir_128[129] = { 
    0 , 0, 0, 0, 0, 0, 0, 0, 1, 0, -1, 0, 1 , 0, -1, 0,  1, 0, -2,-1,  2,  1,- 2, -2,  2,  3, -1, -4,  1,   4,  0,  -5,
    -1, 6, 2,-6,-4, 7, 5,-6,-7, 6, 10,-5,-12, 3, 15,-1,-18,-3, 20, 8,-23,-14, 25, 23,-27,-36, 29, 57,-30,-104, 30, 324, 
    481, 324, 30,-104,-30, 57, 29,-36,-27, 23, 25,-14,-23, 8, 20,-3,-18, -1, 15, 3,-12,-5, 10, 6,-7,-6, 5, 7,-4,-6, 2, 
    6,-1,-5, 0, 4, 1,-4,-1, 3, 2,-2,-2, 1, 2,-1,-2, 0, 1, 0,-1, 0, 1, 0,-1, 0, 1, 0, 0, 0, 0, 0, 0, 0,0
};

int16_t mult_16(filter_16 filter){
    int32_t aux = 0;
    for( int i = 1; i < 16; i++){
        aux += filter.buf[i]*fir_16[i];
    }
    return aux;
}

int16_t mult_32(filter_32 filter){
    int32_t aux = 0;
    for( int i = 1; i < 32; i++){
        aux += filter.buf[i]*fir_32[i];
    }
    return aux;
}

int16_t mult_128(filter_128 filter){
    int32_t aux = 0;
    for( int i = 7; i < 121; i++){
        aux += filter.buf[i]*fir_128[i];
    }
    aux /= 1024;
    return aux;
}

int16_t mean_4(filter_4 filter){
    int32_t aux = 0;
    for(int i=0; i < 4; i++){
        aux += filter.buf[i];
    }
    return aux/4;
}

void add_4(filter_4 *filter, int16_t element) {
    // check if the buffer is full
    if ((filter->tail + 1) % SIZE_4 == filter->head) {
        // buffer is full, overwrite the oldest element
        filter->buf[filter->head] = element;

        
        filter->wait -= 1;
        if(filter->wait == 0){
            filter->wait = IGNORE_SAMPLES;
            filter->next_OK = true;
        }
        
        // increment both head and tail pointers
        filter->head = (filter->head + 1) % SIZE_4;
        filter->tail = (filter->tail + 1) % SIZE_4;
    } else {
        // buffer is not full, insert the new element
        filter->buf[filter->tail] = element;
        // increment the tail pointer
        filter->tail = (filter->tail + 1) % SIZE_4;
    }
};

void add_16(filter_16 *filter, int16_t element) {
    // check if the buffer is full
    if ((filter->tail + 1) % SIZE_16 == filter->head) {
        // buffer is full, overwrite the oldest element
        filter->buf[filter->head] = element;

        
        filter->wait -= 1;
        if(filter->wait == 0){
            filter->wait = IGNORE_SAMPLES;
            filter->next_OK = true;
        }
        
        // increment both head and tail pointers
        filter->head = (filter->head + 1) % SIZE_16;
        filter->tail = (filter->tail + 1) % SIZE_16;
    } else {
        // buffer is not full, insert the new element
        filter->buf[filter->tail] = element;
        // increment the tail pointer
        filter->tail = (filter->tail + 1) % SIZE_16;
    }
};

void add_32(filter_32 *filter, int16_t element) {
    // check if the buffer is full
    if ((filter->tail + 1) % SIZE_32 == filter->head) {
        // buffer is full, overwrite the oldest element
        filter->buf[filter->head] = element;

        filter->wait -= 1;
        if(filter->wait == 0){
            filter->wait = IGNORE_SAMPLES;
            filter->next_OK = true;
        }


        // increment both head and tail pointers
        filter->head = (filter->head + 1) % SIZE_32;
        filter->tail = (filter->tail + 1) % SIZE_32;
    } else {
        // buffer is not full, insert the new element
        filter->buf[filter->tail] = element;
        // increment the tail pointer
        filter->tail = (filter->tail + 1) % SIZE_32;
    }
};

void add_128(filter_128 *filter, int16_t element) {
    // check if the buffer is full
    if ((filter->tail + 1) % SIZE_128 == filter->head) {
        // buffer is full, overwrite the oldest element
        filter->buf[filter->head] = element;

        filter->wait -= 1;
        if(filter->wait == 0){
            filter->wait = IGNORE_SAMPLES;
            filter->next_OK = true;
        }

        // increment both head and tail pointers
        filter->head = (filter->head + 1) % SIZE_128;
        filter->tail = (filter->tail + 1) % SIZE_128;
    } else {
        // buffer is not full, insert the new element
        filter->buf[filter->tail] = element;
        // increment the tail pointer
        filter->tail = (filter->tail + 1) % SIZE_128;
    }
};



