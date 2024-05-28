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

#include "sdrplay_api.h"

#define SAMPLE_RATE 8000000
#define FILENAME_BUFFER_SIZE 80
#define RX_TIME_INTERVAL_THRESHOLD 10
#define DEFAULT_CENTRAL_FREQ 100000000.0
#define DEFAULT_SAMPLING_RATE 8000000.0

int getCurrTime(char *buffer);
void write_to_disk();
typedef void (*data_callback_t)(void*, const char*, int);
void process_data(const char *data, int data_len);
void *socket_listener(void *arg);
int listen_on_socket();
void cleanup_and_exit();

time_t gs_reception_Timestamp;

int masterInitialised = 0;
int slaveUninitialised = 0;

short connected_to_GS = 0;

int curr_file=0;
sdrplay_api_DeviceT *chosenDevice = NULL;
sdrplay_api_ErrT err;
pthread_t socket_thread;
int sockfd;

short IQ[SAMPLE_RATE*2+1] = {0};
int curr = 0, curr_iq=0;

int port = 9090;
double newFreq = DEFAULT_CENTRAL_FREQ;
double newSampling = DEFAULT_SAMPLING_RATE;
int newParam = 0;

void
runSMARTExperiment(sdrplay_api_RxChannelParamsT *chParams){
    sdrplay_api_ErrT err;
    long int elapsed_seconds = 0;
    gs_reception_Timestamp = clock();

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
getCurrTime(char *buffer){
    time_t current_time = time(NULL);

    //printf("Current Unix Timestamp: %ld\n", current_time);

    struct tm * timeinfo;

    timeinfo = localtime(&current_time);

    if (timeinfo == NULL) {
      fprintf(stderr, "Error converting time: %s\n", strerror(errno));
      return 0;
    }

    strftime(buffer, sizeof(buffer)*FILENAME_BUFFER_SIZE, "%Y-%m-%d %H:%M:%S.bin", timeinfo);
    return 1;
}


void 
write_to_disk(){
    char filename[FILENAME_BUFFER_SIZE];

    if(!getCurrTime(filename))
        return;

    // This is to test. 
    // TODO: Remove this and make this change in the transmission to the GS after decimation
    if(connected_to_GS == 1){
        FILE *save_samples;
        save_samples = fopen(filename, "wb");
        fwrite(IQ, 1, sizeof(short)*chosenDevice->rspDuoSampleFreq*2, save_samples);
        printf("[%s] Saved data\n", filename);
        fclose(save_samples);
    }

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
    if (data[0] != ':' || data[2] != ':' || data[data_len-2] != ':')
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


void 
StreamACallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params, unsigned int numSamples, unsigned int reset, void *cbContext)
{
    if (reset)
        printf("sdrplay_api_StreamACallback: numSamples=%d\n", numSamples);

    // Process stream callback data here
    //printf("[ %i / %i]\n", params->firstSampleNum/8000000, numSamples);
    for(int i = 0; i < numSamples; i++){
        if(curr > chosenDevice->rspDuoSampleFreq){
            curr = 0;
            curr_iq = 0;

            write_to_disk();
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

