#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#include "minmea.h"
#define INDENT_SPACES "  "

void decodeNMEA(char *buf[10000]);

static volatile int keepRunning = 1;

FILE *f;
int sockint, s; /* client socket */

void intHandler(int dummy) {
    fclose(f);
    close(s);
    exit(0);
}

main( argc, argv)
int argc;
char **argv;
{       
        signal(SIGINT, intHandler);
        f = fopen("gps.log", "a");
        
        unsigned short port ; /* port number client will connect to */
        struct sockaddr_in server; /* server address */
        char buf[10000]; /* data buffer for sending and receiving */
        

        if( argc != 3 ) { /* check command line arguments */
                fprintf( stderr, "Usage: %s hostname port\n", argv[0] );
                exit(1);
        }

        port = (unsigned short) atoi(argv[2]); /* get the port number */

        char msg[256] = "?WATCH={\"enable\":true,\"nmea\":true};\r\n"; /* create the message */

        /* create stream socket using TCP */
        fprintf(stderr, "Creating datagram socket.\n");
        s = socket(AF_INET, SOCK_STREAM, 0);
        if( s == -1 ) {
                fprintf(stderr, "Socket was not created.\n");
                exit(2);
        }
        else
                fprintf(stderr, "Socket created successfully.\n");

        server.sin_family = AF_INET; /* set up the server name */
        server.sin_port = htons(port);
        server.sin_addr.s_addr = inet_addr( argv[1] );

        /* connect to the server */
        if( connect(s, &server, sizeof(server)) < 0) {
                fprintf(stderr, "Failed to connect to socket.\n");
                exit(3);
        }
        //sleep(1);
        if( recv(s, buf, sizeof(buf), 0) < 0 ) {
            fprintf(stderr, "Failed to receive data from socket.\n");
            exit(5);
        }
        printf("Sending the message: %s\n", msg); /* send the message */
        if( send(s, msg, sizeof(msg), 0) < 0 ) {
                fprintf(stderr, "Failed to send data to socket.\n");
                exit(4);
        }
        int retLen = 0;

        for(int i = 0; i < 10000; i++)
                buf[i] = 0;
        
        int newString = 0;
        int lineChar = 0;
        char NMEA_line[10000] = {0};

        while(1){
                retLen = recv(s, buf, 1, 0);
                if (buf[0] == '$')
                        break;
        }
        NMEA_line[0] = '$';
        lineChar++;
        while(1){
                /* receive the echoed message from the server */
                retLen = recv(s, buf, 1, 0);
                NMEA_line[lineChar] = buf[0];
                lineChar++;
                if( retLen < 0 ) {
                        fprintf(stderr, "Failed to receive data from socket.\n");
                        exit(5);
                }
                //printf("\nNMEA STRING BEFORE PARSING: %s", NMEA_line);
                if(NMEA_line[lineChar-1] == '$'){
                        NMEA_line[lineChar-1] = '\0';
                        fprintf(f, "%s", NMEA_line);
                        //printf("\nNMEA STRING BEFORE PARSING: %s", NMEA_line);
                        decodeNMEA(NMEA_line);
                        for(int i = 0; i < 10000; i++)
                                NMEA_line[i] = 0;
                        lineChar = 0;
                        NMEA_line[lineChar] = '$';
                        lineChar++;
                }
        }
        close(s); /* close the socket */
        printf("Client closed successfully\n"); /* successfully exit */
        exit(0);
}

void decodeNMEA(char *buf[10000]){
        char line[MINMEA_MAX_SENTENCE_LENGTH];
        //line[0] = '$';
        //strcat(line, buf);
        sprintf(line, "%s", buf);
        //printf("LINE=%s", line);
        switch (minmea_sentence_id(line, false)) {
                case MINMEA_SENTENCE_RMC: {
                struct minmea_sentence_rmc frame;
                if (minmea_parse_rmc(&frame, line)) {
                        /*printf(INDENT_SPACES "$xxRMC: raw coordinates and speed: (%d/%d,%d/%d) %d/%d\n",
                                frame.latitude.value, frame.latitude.scale,
                                frame.longitude.value, frame.longitude.scale,
                                frame.speed.value, frame.speed.scale);
                        printf(INDENT_SPACES "$xxRMC fixed-point coordinates and speed scaled to three decimal places: (%d,%d) %d\n",
                                minmea_rescale(&frame.latitude, 1000),
                                minmea_rescale(&frame.longitude, 1000),
                                minmea_rescale(&frame.speed, 1000));*/
                        printf(INDENT_SPACES "Latitude %f Longitude %f Speed %f\n",
                                minmea_tocoord(&frame.latitude),
                                minmea_tocoord(&frame.longitude),
                                minmea_tofloat(&frame.speed));
                }
                else {
                        printf(INDENT_SPACES "$xxRMC sentence is not parsed\n");
                }
                } break;

                case MINMEA_SENTENCE_GGA: {
                struct minmea_sentence_gga frame;
                if (minmea_parse_gga(&frame, line)) {
                        //printf(INDENT_SPACES "$xxGGA: fix quality: %d\n", frame.fix_quality);
                        printf(INDENT_SPACES "Altitude: %d %c\n", minmea_rescale(&frame.altitude, 1), frame.altitude_units);
                }
                else {
                        printf(INDENT_SPACES "$xxGGA sentence is not parsed\n");
                }
                } break;

                case MINMEA_SENTENCE_GST: {
                struct minmea_sentence_gst frame;
                if (minmea_parse_gst(&frame, line)) {
                        /*
                        printf(INDENT_SPACES "$xxGST: raw latitude,longitude and altitude error deviation: (%d/%d,%d/%d,%d/%d)\n",
                                frame.latitude_error_deviation.value, frame.latitude_error_deviation.scale,
                                frame.longitude_error_deviation.value, frame.longitude_error_deviation.scale,
                                frame.altitude_error_deviation.value, frame.altitude_error_deviation.scale);
                        printf(INDENT_SPACES "$xxGST fixed point latitude,longitude and altitude error deviation"
                                " scaled to one decimal place: (%d,%d,%d)\n",
                                minmea_rescale(&frame.latitude_error_deviation, 10),
                                minmea_rescale(&frame.longitude_error_deviation, 10),
                                minmea_rescale(&frame.altitude_error_deviation, 10));
                        printf(INDENT_SPACES "$xxGST floating point degree latitude, longitude and altitude error deviation: (%f,%f,%f)",
                                minmea_tofloat(&frame.latitude_error_deviation),
                                minmea_tofloat(&frame.longitude_error_deviation),
                                minmea_tofloat(&frame.altitude_error_deviation));
                        */
                }
                else {
                        printf(INDENT_SPACES "$xxGST sentence is not parsed\n");
                }
                } break;

                case MINMEA_SENTENCE_GSV: {
                struct minmea_sentence_gsv frame;
                if (minmea_parse_gsv(&frame, line)) {
                        /*
                        printf(INDENT_SPACES "$xxGSV: message %d of %d\n", frame.msg_nr, frame.total_msgs);
                        printf(INDENT_SPACES "$xxGSV: satellites in view: %d\n", frame.total_sats);
                        for (int i = 0; i < 4; i++)
                        printf(INDENT_SPACES "$xxGSV: sat nr %d, elevation: %d, azimuth: %d, snr: %d dbm\n",
                                frame.sats[i].nr,
                                frame.sats[i].elevation,
                                frame.sats[i].azimuth,
                                frame.sats[i].snr);
                        */
                }
                else {
                        printf(INDENT_SPACES "$xxGSV sentence is not parsed\n");
                }
                } break;

                case MINMEA_SENTENCE_VTG: {
                struct minmea_sentence_vtg frame;
                if (minmea_parse_vtg(&frame, line)) {
                        /*
                        printf(INDENT_SPACES "$xxVTG: true track degrees = %f\n",
                                minmea_tofloat(&frame.true_track_degrees));
                        printf(INDENT_SPACES "        magnetic track degrees = %f\n",
                                minmea_tofloat(&frame.magnetic_track_degrees));
                        printf(INDENT_SPACES "        speed knots = %f\n",
                                minmea_tofloat(&frame.speed_knots));
                        printf(INDENT_SPACES "        speed kph = %f\n",
                                minmea_tofloat(&frame.speed_kph));
                        */
                }
                else {
                        printf(INDENT_SPACES "$xxVTG sentence is not parsed\n");
                }
                } break;

                case MINMEA_SENTENCE_ZDA: {
                struct minmea_sentence_zda frame;
                if (minmea_parse_zda(&frame, line)) {
                        printf("\e[1;1H\e[2J");
                        printf(INDENT_SPACES "TIME: %d:%d:%d %02d.%02d.%d UTC%+03d:%02d\n",
                                frame.time.hours+1,
                                frame.time.minutes,
                                frame.time.seconds,
                                frame.date.day,
                                frame.date.month,
                                frame.date.year,
                                frame.hour_offset,
                                frame.minute_offset);
                }
                else {
                        printf(INDENT_SPACES "$xxZDA sentence is not parsed\n");
                }
                } break;

                case MINMEA_INVALID: {
                        //printf(INDENT_SPACES "$xxxxx sentence is not valid\n");
                } break;

                default: {
                        //printf(INDENT_SPACES "$xxxxx sentence is not parsed\n");
                } break;
        }
}