#include <stdio.h>
#include <math.h>
#include "filters.h"
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

#define SAMPLE_RATE 8000000

int16_t IQ[SAMPLE_RATE*2+1] = {0};
int16_t IQ_out[SAMPLE_RATE*2+1] = {0};
int curr = 0, curr_iq=0;

int main (){
    clock_t start, end;
    double cpu_time_used;
    start = clock();
     
    FILE *f = fopen("samples/samples0.bin", "rb");
    FILE *out = fopen("downsampled.bin", "wb");
    
    fread(IQ, sizeof(IQ), 1, f);

    filter_16 round1_i,round1_q;
    filter_16 round2_i,round2_q;
    filter_16 round3_i,round3_q;
    filter_16 round4_i,round4_q;
    filter_16 round5_i,round5_q;
    filter_16 round6_i,round6_q;
    filter_16 round7_i,round7_q;
    filter_16 round8_i,round8_q;
    filter_16 round9_i,round9_q;
    filter_32 round10_i,round10_q;
    filter_128 round11_i,round11_q;
    
    
    for(int i = 0; i < 17; i++){
        round1_i.buf[i] = 0; round1_q.buf[i] = 0;
        round2_i.buf[i] = 0; round2_q.buf[i] = 0;
        round3_i.buf[i] = 0; round3_q.buf[i] = 0;
        round4_i.buf[i] = 0; round4_q.buf[i] = 0;
        round5_i.buf[i] = 0; round5_q.buf[i] = 0;
        round6_i.buf[i] = 0; round6_q.buf[i] = 0;
        round7_i.buf[i] = 0; round7_q.buf[i] = 0;
        round8_i.buf[i] = 0; round8_q.buf[i] = 0;
        round9_i.buf[i] = 0; round9_q.buf[i] = 0;
    }
    
    for(int i = 0; i < 33; i++){
        round10_i.buf[i] = 0; round10_q.buf[i] = 0;
    }

    for(int i = 0; i < 129; i++){
        round11_i.buf[i] = 0; round11_q.buf[i] = 0;
    }

    round1_i.head=0;round1_q.head=0;
    round2_i.head=0;round2_q.head=0;
    round3_i.head=0;round3_q.head=0;
    round4_i.head=0;round4_q.head=0;
    round5_i.head=0;round5_q.head=0;
    round6_i.head=0;round6_q.head=0;
    round7_i.head=0;round7_q.head=0;
    round8_i.head=0;round8_q.head=0;
    round9_i.head=0;round9_q.head=0;
    round10_i.head=0;round10_q.head=0;
    round11_i.head=0;round11_q.head=0;      

    round1_i.tail=0;round1_q.tail=0;
    round2_i.tail=0;round2_q.tail=0;
    round3_i.tail=0;round3_q.tail=0;
    round4_i.tail=0;round4_q.tail=0;
    round5_i.tail=0;round5_q.tail=0;
    round6_i.tail=0;round6_q.tail=0;
    round7_i.tail=0;round7_q.tail=0;
    round8_i.tail=0;round8_q.tail=0;
    round9_i.tail=0;round9_q.tail=0;
    round10_i.tail=0;round10_q.tail=0;
    round11_i.tail=0;round11_q.tail=0; 

    round1_i.curr_s=0;round1_q.curr_s=0;
    round2_i.curr_s=0;round2_q.curr_s=0;
    round3_i.curr_s=0;round3_q.curr_s=0;
    round4_i.curr_s=0;round4_q.curr_s=0;
    round5_i.curr_s=0;round5_q.curr_s=0;
    round6_i.curr_s=0;round6_q.curr_s=0;
    round7_i.curr_s=0;round7_q.curr_s=0;
    round8_i.curr_s=0;round8_q.curr_s=0;
    round9_i.curr_s=0;round9_q.curr_s=0;
    round10_i.curr_s=0;round10_q.curr_s=0;
    round11_i.curr_s=0;round11_q.curr_s=0; 

    round1_i.next_OK=false;round1_q.next_OK=false;
    round2_i.next_OK=false;round2_q.next_OK=false;
    round3_i.next_OK=false;round3_q.next_OK=false;
    round4_i.next_OK=false;round4_q.next_OK=false;
    round5_i.next_OK=false;round5_q.next_OK=false;
    round6_i.next_OK=false;round6_q.next_OK=false;
    round7_i.next_OK=false;round7_q.next_OK=false;
    round8_i.next_OK=false;round8_q.next_OK=false;
    round9_i.next_OK=false;round9_q.next_OK=false;
    round10_i.next_OK=false;round10_q.next_OK=false;
    round11_i.next_OK=false;round11_q.next_OK=false;

    round1_i.wait=IGNORE_SAMPLES;round1_q.wait=IGNORE_SAMPLES;
    round2_i.wait=IGNORE_SAMPLES;round2_q.wait=IGNORE_SAMPLES;
    round3_i.wait=IGNORE_SAMPLES;round3_q.wait=IGNORE_SAMPLES;
    round4_i.wait=IGNORE_SAMPLES;round4_q.wait=IGNORE_SAMPLES;
    round5_i.wait=IGNORE_SAMPLES;round5_q.wait=IGNORE_SAMPLES;
    round6_i.wait=IGNORE_SAMPLES;round6_q.wait=IGNORE_SAMPLES;
    round7_i.wait=IGNORE_SAMPLES;round7_q.wait=IGNORE_SAMPLES;
    round8_i.wait=IGNORE_SAMPLES;round8_q.wait=IGNORE_SAMPLES;
    round9_i.wait=IGNORE_SAMPLES;round9_q.wait=IGNORE_SAMPLES;
    round10_i.wait=IGNORE_SAMPLES;round10_q.wait=IGNORE_SAMPLES;
    round11_i.wait=IGNORE_SAMPLES;round11_q.wait=IGNORE_SAMPLES;

    int samples_out = 0;

    // nos ifs substituir: round1_i.curr_s == SIZE_16-1 -> round1_i.next_OK
    for(int sample = 0; sample < SAMPLE_RATE*2+1; sample+=2){
        add_16(&round1_i, IQ[sample]); add_16(&round1_q, IQ[sample+1]);
        //printf("round1_i.curr_s = %i | round1_i.head = %i | round1_i.tail = %i \n", round1_i.curr_s, round1_i.head, round1_i.tail);
        //printf("round2_i.curr_s = %i | round2_i.head = %i | round2_i.tail = %i \n", round2_i.curr_s, round2_i.head, round2_i.tail);
        if (round1_i.next_OK){
            round1_i.next_OK = false;
            add_16(&round2_i, mult_16(round1_i)); add_16(&round2_q, mult_16(round1_q));
        }

        if (round2_i.next_OK){
            round2_i.next_OK = false;
            add_16(&round3_i, mult_16(round2_i)); add_16(&round3_q, mult_16(round2_q));
        }

        if (round3_i.next_OK){
            round3_i.next_OK = false;
            add_16(&round4_i, mult_16(round3_i)); add_16(&round4_q, mult_16(round3_q));
        }

        if (round4_i.next_OK){
            round4_i.next_OK = false;
            add_16(&round5_i, mult_16(round4_i)); add_16(&round5_q, mult_16(round4_q));
        }

        if (round5_i.next_OK){
            round5_i.next_OK = false;
            add_16(&round6_i, mult_16(round5_i)); add_16(&round6_q, mult_16(round5_q));
        }

        if (round6_i.next_OK){
            round6_i.next_OK = false;
            add_16(&round7_i, mult_16(round6_i)); add_16(&round7_q, mult_16(round6_q));
        }

        if (round7_i.next_OK){
            round7_i.next_OK = false;
            add_16(&round8_i, mult_16(round7_i)); add_16(&round8_q, mult_16(round7_q));
        }

        if (round8_i.next_OK){
            round8_i.next_OK = false;
            add_16(&round9_i, mult_16(round8_i)); add_16(&round9_q, mult_16(round8_q));
        }

        if (round9_i.next_OK){
            round9_i.next_OK = false;
            add_32(&round10_i, mult_16(round9_i)); add_32(&round10_q, mult_16(round9_q));
        }

        if (round10_i.next_OK){
            round10_i.next_OK = false;
            add_128(&round11_i, mult_32(round10_i)); add_128(&round11_q, mult_32(round10_q));
        }

        if (round11_i.next_OK){
            round11_i.next_OK = false;
            IQ_out[samples_out] = mult_128(round11_i);
            samples_out++;
            IQ_out[samples_out] = mult_128(round11_q);
            samples_out++;
            //printf("[%.2f%%] Written samples %i/%i\n", (float)sample/sizeof(IQ)*200.0, IQ_out[samples_out-2], IQ_out[samples_out-1]);
            //exit(0);
        }
    }
    
    printf("Samples out = %i/%i\n", samples_out, SAMPLE_RATE*2);
    fwrite(IQ_out, sizeof(int16_t)*samples_out,1, out);

    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

    printf("\nTime: %.2f s\n", cpu_time_used);
    fclose(f);
    fclose(out);
    return 0;
}