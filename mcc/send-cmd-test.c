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
#include <arpa/inet.h>

#define SERVER_ADDRESS "192.168.70.200"  
#define SERVER_PORT 9090               

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in server_addr;

    if( argc < 2 || argc > 3){
        fprintf(stderr, "Usage: %s <command> <value>\n", argv[0]);
        exit(1);
    }

    char command = argv[1][0];  
    int value = 0; 

    if (argc == 3){
        value = atoi(argv[2]);     
    }
    
    int max_value_length = 12;  
    int string_length =0;

    if(command != 'H' && command != 'U' && command != 'Z' && command != 'R')
        string_length = 3 + 1 + strlen(argv[2]) + max_value_length + 2;  
    else
        string_length = 3 + 1 + max_value_length + 2;  
    char formatted_string[string_length];
    
    if (command == 'H' || command == 'U' || command == 'Z'|| command == 'R')
        snprintf(formatted_string, string_length, ":%c:", command);
    else
        snprintf(formatted_string, string_length, ":%c:%d:", command, value);

    printf("Formatted string: %s\n", formatted_string);
    printf("length/max length = %li/%i\n", strlen(formatted_string), string_length);


    sockfd = socket(AF_INET, SOCK_STREAM, 0);  
    if (sockfd < 0) {
    perror("socket");
    exit(1);
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    server_addr.sin_port = htons(SERVER_PORT);
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sockfd);
        exit(1);
    }
    int bytes_sent = send(sockfd, formatted_string, strlen(formatted_string), 0);
    if (bytes_sent < 0) {
        perror("send");
        close(sockfd);
        exit(1);
    }

    printf("Sent %d bytes of data to OBC-P1.\n", bytes_sent);
    close(sockfd);

    return 0;
}
