#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h> 
#include <strings.h> 
#include <sys/types.h> 
#include <arpa/inet.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <unistd.h> 
#include <stdlib.h> 
  
#define PORT 5000 
#define MAXLINE 1000 

#include "sdrplay_api.h"
#include "filters.h"

#include "sinCosTable.h"

#define SAMPLE_RATE 8000000
#define FILENAME_BUFFER_SIZE 256
#define RX_TIME_INTERVAL_THRESHOLD 10
#define DEFAULT_CENTRAL_FREQ 100000000.0
#define DEFAULT_SAMPLING_RATE 8000000.0
#define WRITE_TO_DISK_INTERVAL 5

int getDownsampleFileName(char *buffer, int downsample);
void write_to_disk();
typedef void (*data_callback_t)(void*, const char*, int);
void process_data(const char *data, int data_len);
void *socket_listewriner(void *arg);
int listen_on_socket();
void cleanup_and_exit();

time_t gs_reception_Timestamp;

int masterInitialised = 0;
int slaveUninitialised = 0;

short connected_to_GS = 0;
int samples_out = 0;

int curr_file=0;
sdrplay_api_DeviceT *chosenDevice = NULL;
sdrplay_api_ErrT err;
pthread_t socket_thread;
int sockfd;

short IQ[SAMPLE_RATE*2+1] = {0};
int curr = 0, curr_iq=0;
int samples_debug = 0;

int port = 9090;
double newFreq = DEFAULT_CENTRAL_FREQ;
double newSampling = DEFAULT_SAMPLING_RATE;
int newParam = 0;

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
int16_t IQ_out[SAMPLE_RATE*2+1] = {0};

int sinIndex = 0;
int cosIndex = 20000;

int sockfd_data, n; 
struct sockaddr_in servaddr_data; 

time_t last_file_create;
time_t current_time ;

// TODO: Start filename with "/home/smart/rlsync/"
char filename_downsample[FILENAME_BUFFER_SIZE];
char filename_raw[FILENAME_BUFFER_SIZE];
int written_samples = 160000000; //start at 16M to create first file
int written_raw = 16000000; // same as above

void
runSMARTExperiment(sdrplay_api_RxChannelParamsT *chParams){
    sdrplay_api_ErrT err;
    long int elapsed_seconds = 0;
    gs_reception_Timestamp = clock();

    // change this later
    
      /*
    // clear servaddr 
    bzero(&servaddr_data, sizeof(servaddr_data)); 
    servaddr_data.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    servaddr_data.sin_port = htons(5000); 
    servaddr_data.sin_family = AF_INET; 
      
    // create datagram socket 
    sockfd_data = socket(AF_INET, SOCK_DGRAM, 0); 
      
    // connect to server 
    if(connect(sockfd_data, (struct sockaddr *)&servaddr_data, sizeof(servaddr_data)) < 0) 
    { 
        printf("\n Error : Connect Failed \n"); 
        exit(0); 
    } */
    //------------
    
    //  Not needed, default is 8M but good to have for debug
    chParams->tunerParams.rfFreq.rfHz = DEFAULT_CENTRAL_FREQ;
    chosenDevice->rspDuoSampleFreq = DEFAULT_SAMPLING_RATE;
    
    // Prepare socket thread
    listen_on_socket();
    double t = 0;

    // Create ctrl+c and other signal catchers. This is needed to unititialize API 
    signal(SIGINT, cleanup_and_exit);
    signal(SIGSEGV, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);
    signal(SIGKILL, cleanup_and_exit);
    
    while (1) 
    {   
        printf("\e[1;1H\e[2J");
        printf("     %s Connected to GS  [%ld s]           \n", connected_to_GS == 1 ? "" : "NOT", elapsed_seconds);
        printf("Current SDR Config:\n");
        printf("Central frequency: %.3f MHz\n", chParams->tunerParams.rfFreq.rfHz/1e6);
        printf("Gain             : %i dB\n", chParams->tunerParams.gain.gRdB);
        printf("Sampling frequency: %.0f MSPS\n", chosenDevice->rspDuoSampleFreq/1e6);
        printf("Samples debug: %i\n", samples_debug/8000000);

        if ( newParam ){
            // newParam may generate race condition?
            newParam = 0;
            chParams->tunerParams.rfFreq.rfHz = newFreq;
            chosenDevice->rspDuoSampleFreq = newSampling;

            if ((err = sdrplay_api_Update(chosenDevice->dev, chosenDevice->tuner, sdrplay_api_Update_Tuner_Frf, sdrplay_api_Update_Ext1_None)) != sdrplay_api_Success)
            {
                printf("sdrplay_api_Update sdrplay_api_Update_Tuner_Gr failed %s\n", sdrplay_api_GetErrorString(err));
                break;
            }
        }
        
        t = (double)(time(NULL) - gs_reception_Timestamp);
        printf("Time since last reception: %.2f\n", t);

        //  Only send to GS if time since last cmd reception < TIME_THRESHOLD
        if (t > RX_TIME_INTERVAL_THRESHOLD){
            connected_to_GS = 0;
        }
        else{
            connected_to_GS = 1;
            printf("\nSending to GS...\n");
        }

        elapsed_seconds++;
        sleep(1);
    }
}

void 
cleanup_and_exit(){
    printf("[cleanup]\n");
    // Close socket
    close(sockfd);
    printf("Socket closed.\n");

    // Finished with device so uninitialise it
    sdrplay_api_Uninit(chosenDevice->dev);
    printf("Socket closed.\n");

    // Release device (make it available to other applications)
    sdrplay_api_ReleaseDevice(chosenDevice);
    printf("Device released.\n");
    
    // Unlock API
    sdrplay_api_UnlockDeviceApi();
    printf("API Unlocked.\n");

    // Close API
    sdrplay_api_Close();
    printf("API closed.\n");

    exit(0);
}

int 
getDownsampleFileName(char *buffer, int downsample){
    current_time = time(NULL);

    if(downsample){
        if (written_samples < 7812)
            return 1;
    }
    else{
        if (written_raw < 16000000)
            return 1;
    }
    //printf("Current Unix Timestamp: %ld\n", current_time);
    last_file_create = current_time;
    struct tm * timeinfo;

    timeinfo = localtime(&current_time);

    if (timeinfo == NULL) {
      fprintf(stderr, "Error converting time: %s\n", strerror(errno));
      return 0;
    }
    char time_str[FILENAME_BUFFER_SIZE];
    strftime(time_str, sizeof(time_str)*FILENAME_BUFFER_SIZE, "%Y-%m-%d %H:%M:%S", timeinfo);
    char aux[30];

    if(downsample){
        sprintf(aux, "_DOWNSAMPLED_f=%.3f.bin", newFreq/1000000);
    }
    else{
        sprintf(aux, "RAW_f=%.3f.bin", newFreq/1000000);
    }
    //strcat(buffer, aux);

    /*  TODO: distinguish between downsample and raw for the filename */
    char downsample_path[FILENAME_BUFFER_SIZE] = "downsampled/";
    char raw_path[FILENAME_BUFFER_SIZE] = "raw/";
    if(downsample){
        strcat(downsample_path, time_str);
        strcat(downsample_path, aux);
        sprintf(buffer, "%s", downsample_path);
    }
    else{
        strcat(raw_path, time_str);
        strcat(raw_path, aux);
        sprintf(buffer, "%s", raw_path);
    }
    
   if(downsample)
        written_samples = 0;
    else
        written_raw = 0;
    return 1;
}


void 
write_to_disk(short *xi, short *xq, int numSamples){

    if(!getDownsampleFileName(filename_downsample, 1))
        return;

    for(int sample = 0; sample < numSamples; sample++){
        add_16(&round1_i, (int16_t)((float)IQ[sample]*sinCosTable[sinIndex])); add_16(&round1_q, (int16_t)((float)IQ[sample+1]*sinCosTable[cosIndex]));
        sinIndex = sinIndex+1 == 80000 ? 0 : sinIndex+1;
        cosIndex = cosIndex+1 == 80000 ? 0 : cosIndex+1;
        //printf("samples_out %i \t sample %i\n", samples_out, sample);
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
            if(samples_out > 16000000)
                samples_out = 0;
            IQ_out[samples_out] = mult_128(round11_i);
            samples_out++;
            IQ_out[samples_out] = mult_128(round11_q);
            samples_out++;
            int16_t out_i = mult_128(round11_i);
            int16_t out_q = mult_128(round11_q);
            if(connected_to_GS == 1){
                FILE *save_samples;
                save_samples = fopen(filename_downsample, "ab");
                fwrite(&out_i, sizeof(int16_t), 1, save_samples);
                fwrite(&out_q, sizeof(int16_t), 1, save_samples);
                fclose(save_samples);
                written_samples+=2;
            }
        }
        
        samples_debug++;
    }
        /*
    if(connected_to_GS == 1){
        //printf("Downsample and send to GS...\n");
        //FILE *save_samples;
        //save_samples = fopen(filename, "wb");
        //fwrite(IQ, 1, sizeof(short)*chosenDevice->rspDuoSampleFreq*2, save_samples);
        //printf("[%s] Saved data\n", filename);
        //fclose(save_samples);
        // Open socket to GS
        
      
        // request to send datagram 
        // no need to specify server address in sendto 
        // connect stores the peers IP and port 
        
        // Perform downsample
        

        
        // Send data
        //printf("samples_out: %i\n", samples_out);
        sendto(sockfd_data, IQ_out, sizeof(int16_t)*samples_out, 0, (struct sockaddr*)NULL, sizeof(servaddr_data)); 
        samples_out = 0;
        //printf("Sent to GS\n");
    }
        */
       

    curr_file++;
    if (curr_file == 10){
        //exit(0);
    }
    curr = 0;
}

void process_data(const char *data, int data_len) {
    printf("Received data: %s (length: %d)\n", data, data_len);
    // Data format  :F:88000000: - central frequency
    //              :S:6000000:  - sampling rate
    //              :H:          - Heartbeat
    if(data_len < 3) // Minimum msg length
        return;

    // Wrong format
    if (data[0] != ':' || data[2] != ':' || data[data_len-1] != ':')
        return;
    
    // Store param value in aux
    char aux[data_len];
    int i=0;
    switch (data[1])
    {
    case 'H':
        printf("Rcvd heartbeat\n");

        gs_reception_Timestamp = time(NULL);
        break;
    case 'F':
        i=0; while(data[i+3] != ':'){aux[i]=data[i+3]; i++;}
        newFreq = atof(aux) > 2000000 ? atof(aux) : newFreq;

        gs_reception_Timestamp = time(NULL);
        newParam = 1;
        break;
    case 'S':
        i=0; while(data[i+3] != ':'){aux[i]=data[i+3]; i++;}
        newSampling = atof(aux) > 1000000 ? atof(aux) : newSampling;

        gs_reception_Timestamp = time(NULL);
        newParam = 1;
        break;
    case 'U':
        // Turn off USB
        // uhubctl -l 1-1 -p 3 -a off
        system("uhubctl -l 1-1 -p 3 -a off");
        
        break;
    case 'R':
        system("systemctl start restart-sdr-usb.service");
        break;
    default:
        printf("Wrong format\n");
        break;
    }
    

}

void *socket_listener(void *arg) {
    struct sockaddr_in server_addr;
    // Create a socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);  // TCP socket
    if (sockfd < 0) {
        perror("socket");
        return NULL;
    }

    // Set up server address information
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Listen on all interfaces
    server_addr.sin_port = htons(port);  // Port to listen on

    // Bind the socket to the address
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sockfd);
        return NULL;
    }

    // Listen for incoming connections
    if (listen(sockfd, 5) < 0) {  // Backlog queue for 5 connections
        perror("listen");
        close(sockfd);
        return NULL;
    }
    // Extract arguments
    
    char buffer[1024];  // Adjust buffer size as needed
    printf("Socket listener created\n");
    
    while (1) {
        // Receive data from the socket
        //printf("waiting....\n");
        int fd=accept(sockfd,NULL,NULL);
        int bytes_received = recv(fd, buffer, sizeof(buffer), 0);
        
        if (bytes_received == 0) {
            // Connection closed by peer
            printf("Connection closed by peer.\n");
            break;
        } else if (bytes_received < 0) {
            perror("recv");
            break;
        }
        printf("Received bytes!\n");
        // Call the callback function with received data
        process_data(buffer, bytes_received);
        
        memset(buffer, 0, sizeof(buffer));
    }

    // Close the socket
    close(sockfd);

    return NULL;
}

int listen_on_socket() {
  // Create a thread to listen on the socket
  
  void *args[2];
  args[0] = NULL;
  args[1] = NULL;  // Assuming process_data is defined elsewhere

  int ret = pthread_create(&socket_thread, NULL, socket_listener, args);
  if (ret != 0) {
    perror("pthread_create");
    return -1;
  }

  // Detach the thread (optional)
  // pthread_detach(thread);

  return 0;
}

int time_since_last_disk_write(){
    return time(NULL) - last_file_create;
}

void 
StreamACallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params, unsigned int numSamples, unsigned int reset, void *cbContext)
{
    if (reset)
        printf("sdrplay_api_StreamACallback: numSamples=%d\n", numSamples);

    //if(time_since_last_disk_write() > WRITE_TO_DISK_INTERVAL)
    write_to_disk(xi, xq, numSamples);

    getDownsampleFileName(filename_raw, 0);
    
    FILE *f_raw = fopen(filename_raw, "ab");
    for(int j = 0; j < numSamples; j++){
        fwrite(&xi[j], sizeof(int16_t), 1, f_raw);
        written_raw++;
        fwrite(&xq[j], sizeof(int16_t), 1, f_raw);
        written_raw++;
    }
    fclose(f_raw);

    /*if(written_raw == 16000000){
        written_raw = 0;
        FILE *f_raw = fopen(filename_raw, "wb");
        fwrite(IQ, sizeof(int16_t)*16000000, 1, f_raw);
        fclose(f_raw);
    }*/
    //printf("Received samples %i\n", numSamples);
    //sleep(1);
    // Process stream callback data here
    //printf("[ %i / %i]\n", params->firstSampleNum/8000000, numSamples);
    /*
    for(int i = 0; i < numSamples; i++){
        if(curr > chosenDevice->rspDuoSampleFreq){
            curr = 0;
            curr_iq = 0;

            write_to_disk(xi, xq);
            break;
        }
        //xi_buff[curr] = xi[i];
        //xq_buff[curr] = xq[i];
        IQ[curr_iq] = xi[i];
        curr_iq++;
        IQ[curr_iq] = xq[i];
        curr_iq++;

        curr++;
    }
    */
    return;
}

void 
StreamBCallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params, unsigned int numSamples, unsigned int reset, void *cbContext)
{
    if (reset)
        printf("sdrplay_api_StreamBCallback: numSamples=%d\n", numSamples);

    // Process stream callback data here - this callback will only be used in dual tuner mode

    return;
}

void 
EventCallback(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner, sdrplay_api_EventParamsT *params, void *cbContext)
{
    switch(eventId)
    {
    case sdrplay_api_GainChange:
        printf("sdrplay_api_EventCb: %s, tuner=%s gRdB=%d lnaGRdB=%d systemGain=%.2f\n", "sdrplay_api_GainChange", (tuner == sdrplay_api_Tuner_A)? "sdrplay_api_Tuner_A": "sdrplay_api_Tuner_B", params->gainParams.gRdB, params->gainParams.lnaGRdB, params->gainParams.currGain);
        break;

    case sdrplay_api_PowerOverloadChange:
        printf("sdrplay_api_PowerOverloadChange: tuner=%s powerOverloadChangeType=%s\n", (tuner == sdrplay_api_Tuner_A)? "sdrplay_api_Tuner_A": "sdrplay_api_Tuner_B", (params->powerOverloadParams.powerOverloadChangeType == sdrplay_api_Overload_Detected)? "sdrplay_api_Overload_Detected": "sdrplay_api_Overload_Corrected");
        // Send update message to acknowledge power overload message received
        sdrplay_api_Update(chosenDevice->dev, tuner, sdrplay_api_Update_Ctrl_OverloadMsgAck, sdrplay_api_Update_Ext1_None);
        break;

    case sdrplay_api_RspDuoModeChange:
        printf("sdrplay_api_EventCb: %s, tuner=%s modeChangeType=%s\n", "sdrplay_api_RspDuoModeChange", (tuner == sdrplay_api_Tuner_A)? "sdrplay_api_Tuner_A": "sdrplay_api_Tuner_B", (params->rspDuoModeParams.modeChangeType == sdrplay_api_MasterInitialised)? "sdrplay_api_MasterInitialised": 
                                                                                                                                                                                    (params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveAttached)? "sdrplay_api_SlaveAttached": 
                                                                                                                                                                                    (params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveDetached)? "sdrplay_api_SlaveDetached": 
                                                                                                                                                                                    (params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveInitialised)? "sdrplay_api_SlaveInitialised": 
                                                                                                                                                                                    (params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveUninitialised)? "sdrplay_api_SlaveUninitialised": 
                                                                                                                                                                                    "unknown type");
        if (params->rspDuoModeParams.modeChangeType == sdrplay_api_MasterInitialised)
        {
            masterInitialised = 1;
        }
        if (params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveUninitialised)
        {
            slaveUninitialised = 1;
        }
        break;

    case sdrplay_api_DeviceRemoved:
        printf("sdrplay_api_EventCb: %s\n", "sdrplay_api_DeviceRemoved");
        exit(0);
        break;

    default:
        printf("sdrplay_api_EventCb: %d, unknown event\n", eventId);
        break;
    }
}

void 
usage(void)
{
    printf("Usage: ./OBC-P1 [central frequency]\n");
    exit(1);
}


int 
main(int argc, char *argv[])
{
    // --------------- Filters
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
    
    // --------------- Filters
    sdrplay_api_DeviceT devs[6];
    unsigned int ndev; 
    int i;
    float ver = 0.0;
    sdrplay_api_DeviceParamsT *deviceParams = NULL;
    sdrplay_api_CallbackFnsT cbFns;
    sdrplay_api_RxChannelParamsT *chParams;

    int reqTuner = 0;
    int master_slave = 0;

    double tunerCentralFreq = 0;
    //double tunerBW = 0;

    unsigned int chosenIdx = 0;

    if (argc == 2)
    {
        // Set:
        //  - Central Frequency
        //  - Bandwidth
        //  - Gain
        //  - Fs
        tunerCentralFreq = atof(argv[1]);
    }
    else if (argc >= 4)
    {
        usage();
    }

    printf("requested Tuner%c Mode=%s\n", (reqTuner == 0)? 'A': 'B', (master_slave == 0)? "Single_Tuner": "Master/Slave");

    // Open API
    if ((err = sdrplay_api_Open()) != sdrplay_api_Success)
    {
        printf("sdrplay_api_Open failed %s\n", sdrplay_api_GetErrorString(err));
    }
    else
    {
        // Enable debug logging output
        if ((err = sdrplay_api_DebugEnable(NULL, 1)) != sdrplay_api_Success)
        {
            printf("sdrplay_api_DebugEnable failed %s\n", sdrplay_api_GetErrorString(err));
        }

        // Check API versions match
        if ((err = sdrplay_api_ApiVersion(&ver)) != sdrplay_api_Success)
        {
            printf("sdrplay_api_ApiVersion failed %s\n", sdrplay_api_GetErrorString(err));
        }
        if (ver != SDRPLAY_API_VERSION)
        {
            printf("API version don't match (local=%.2f dll=%.2f)\n", SDRPLAY_API_VERSION, ver);
            goto CloseApi;
        }

        // Lock API while device selection is performed
        sdrplay_api_LockDeviceApi();

        // Fetch list of available devices
        if ((err = sdrplay_api_GetDevices(devs, &ndev, sizeof(devs) / sizeof(sdrplay_api_DeviceT))) != sdrplay_api_Success)
        {
            printf("sdrplay_api_GetDevices failed %s\n", sdrplay_api_GetErrorString(err));
            goto UnlockDeviceAndCloseApi;
        }
        printf("MaxDevs=%ld NumDevs=%d\n", sizeof(devs) / sizeof(sdrplay_api_DeviceT), ndev);
        if (ndev > 0)
        {
            for (i = 0; i < (int)ndev; i++)
            {
                if (devs[i].hwVer == SDRPLAY_RSPduo_ID)
                    printf("Dev%d: SerNo=%s hwVer=%d tuner=0x%.2x rspDuoMode=0x%.2x\n", i, devs[i].SerNo, devs[i].hwVer , devs[i].tuner, devs[i].rspDuoMode);
                else
                    printf("Dev%d: SerNo=%s hwVer=%d tuner=0x%.2x\n", i, devs[i].SerNo, devs[i].hwVer, devs[i].tuner);
            }

            // Choose device
            if ((reqTuner == 1) || (master_slave == 1))  // requires RSPduo
            {
                // Pick first RSPduo
                for (i = 0; i < (int)ndev; i++)
                {
                    if (devs[i].hwVer == SDRPLAY_RSPduo_ID)
                    {
                        chosenIdx = i;
                        break;
                    }
                }
            }
            else
            {
                // Pick first device of any type
                for (i = 0; i < (int)ndev; i++)
                {
                    chosenIdx = i;
                    break;
                }
            }
            if (i == ndev)
            {
                printf("Couldn't find a suitable device to open - exiting\n");
                goto UnlockDeviceAndCloseApi;
            }
            printf("chosenDevice = %d\n", chosenIdx);
            chosenDevice = &devs[chosenIdx];

            // If chosen device is an RSPduo, assign additional fields
            if (chosenDevice->hwVer == SDRPLAY_RSPduo_ID)
            {
                if (chosenDevice->rspDuoMode & sdrplay_api_RspDuoMode_Master)  // If master device is available, select device as master
                {
                    // Select tuner based on user input (or default to TunerA)
                    chosenDevice->tuner = sdrplay_api_Tuner_A;
                    if (reqTuner == 1) 
                        chosenDevice->tuner = sdrplay_api_Tuner_B;

                    // Set operating mode
                    if (!master_slave)  // Single tuner mode
                    {
                        chosenDevice->rspDuoMode = sdrplay_api_RspDuoMode_Single_Tuner;
                        printf("Dev%d: selected rspDuoMode=0x%.2x tuner=0x%.2x\n", chosenIdx, chosenDevice->rspDuoMode, chosenDevice->tuner);
                    }
                    else
                    {
                        chosenDevice->rspDuoMode = sdrplay_api_RspDuoMode_Master;
                        chosenDevice->rspDuoSampleFreq = 6000000.0;  // Need to specify sample frequency in master/slave mode
                        printf("Dev%d: selected rspDuoMode=0x%.2x tuner=0x%.2x rspDuoSampleFreq=%.1f\n", chosenIdx, chosenDevice->rspDuoMode, chosenDevice->tuner, chosenDevice->rspDuoSampleFreq);
                    }
                }
                else  // Only slave device available
                {
                    // Shouldn't change any parameters for slave device
                }
            }

            // Select chosen device
            if ((err = sdrplay_api_SelectDevice(chosenDevice)) != sdrplay_api_Success)
            {
                printf("sdrplay_api_SelectDevice failed %s\n", sdrplay_api_GetErrorString(err));
                goto UnlockDeviceAndCloseApi;
            }

            // Unlock API now that device is selected
            //sdrplay_api_UnlockDeviceApi();

            // Retrieve device parameters so they can be changed if wanted
            if ((err = sdrplay_api_GetDeviceParams(chosenDevice->dev, &deviceParams)) != sdrplay_api_Success)
            {
                printf("sdrplay_api_GetDeviceParams failed %s\n", sdrplay_api_GetErrorString(err));
                goto CloseApi;
            }

            // Check for NULL pointers before changing settings
            if (deviceParams == NULL)
            {
                printf("sdrplay_api_GetDeviceParams returned NULL deviceParams pointer\n");
                goto CloseApi;
            }

            // Configure dev parameters
            if (deviceParams->devParams != NULL) // This will be NULL for slave devices as only the master can change these parameters
            {
                // Only need to update non-default settings
                if (master_slave == 0)
                {
                    // Change from default Fs  to 8MHz
                    deviceParams->devParams->fsFreq.fsHz = 8000000.0;
                }
                else 
                {
                    // Can't change Fs in master/slave mode
                }
            }

            // Configure tuner parameters (depends on selected Tuner which set of parameters to use)
            chParams = (chosenDevice->tuner == sdrplay_api_Tuner_B)? deviceParams->rxChannelB: deviceParams->rxChannelA;
            if (chParams != NULL)
            {   
                if (tunerCentralFreq > 1)
                    chParams->tunerParams.rfFreq.rfHz = tunerCentralFreq;

                chParams->tunerParams.bwType = sdrplay_api_BW_1_536;
                if (master_slave == 0) // Change single tuner mode to ZIF
                {
                    chParams->tunerParams.ifType = sdrplay_api_IF_Zero;
                }
                chParams->tunerParams.gain.gRdB = 40;
                chParams->tunerParams.gain.LNAstate = 5;

                // Disable AGC
                chParams->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;
            }
            else
            {
                printf("sdrplay_api_GetDeviceParams returned NULL chParams pointer\n");
                goto CloseApi;
            }

            // Assign callback functions to be passed to sdrplay_api_Init()
            cbFns.StreamACbFn = StreamACallback;
            cbFns.StreamBCbFn = StreamBCallback;
            cbFns.EventCbFn = EventCallback;

            // Now we're ready to start by calling the initialisation function
            // This will configure the device and start streaming
            if ((err = sdrplay_api_Init(chosenDevice->dev, &cbFns, NULL)) != sdrplay_api_Success)
            {
                printf("sdrplay_api_Init failed %s\n", sdrplay_api_GetErrorString(err));
                if (err == sdrplay_api_StartPending) // This can happen if we're starting in master/slave mode as a slave and the master is not yet running
                {   
                    while(1)
                    {
                        sleep(1000);
                        if (masterInitialised) // Keep polling flag set in event callback until the master is initialised
                        {
                            // Redo call - should succeed this time
                            if ((err = sdrplay_api_Init(chosenDevice->dev, &cbFns, NULL)) != sdrplay_api_Success)
                            {
                                printf("sdrplay_api_Init failed %s\n", sdrplay_api_GetErrorString(err));
                            }
                            goto CloseApi;
                        }
                        printf("Waiting for master to initialise\n");
                    }
                }
                else
                {
                    goto CloseApi;
                }
            }

            runSMARTExperiment(chParams);
		
            // Finished with device so uninitialise it
            if ((err = sdrplay_api_Uninit(chosenDevice->dev)) != sdrplay_api_Success)
            {	
                printf("sdrplay_api_Uninit failed %s\n", sdrplay_api_GetErrorString(err));
                if (err == sdrplay_api_StopPending) // This can happen if we're stopping in master/slave mode as a master and the slave is still running
                {
                    while(1)
                    {
                        sleep(1000);
                        if (slaveUninitialised) // Keep polling flag set in event callback until the slave is uninitialised
                        {
                            // Repeat call - should succeed this time
                            if ((err = sdrplay_api_Uninit(chosenDevice->dev)) != sdrplay_api_Success)
                            {
                                printf("sdrplay_api_Uninit failed %s\n", sdrplay_api_GetErrorString(err));
                            }
                            slaveUninitialised = 0;
                            goto CloseApi;
                        }
                        printf("Waiting for slave to uninitialise\n");
                    }
                }
                goto CloseApi;
            }
            // Release device (make it available to other applications)
            sdrplay_api_ReleaseDevice(chosenDevice);
        }
UnlockDeviceAndCloseApi:
        // Unlock API
        sdrplay_api_UnlockDeviceApi();

CloseApi:
        // Close API
        sdrplay_api_Close();
    }

    return 0;
}

