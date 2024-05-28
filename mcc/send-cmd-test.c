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

#define SERVER_ADDRESS "127.0.0.1"  // Localhost IP address
#define SERVER_PORT 9090               // Port to connect to

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in server_addr;

    if( argc < 2 || argc > 3){
        fprintf(stderr, "Usage: %s <command> <value>\n", argv[0]);
        exit(1);
    }

    char delimiter = argv[1][0];  // Extract the first character as delimiter
    int value = 0; 

    if (argc == 3){
        value = atoi(argv[2]);     // Convert the second argument to integer
    }
    
    // Create the formatted string with appropriate size
    int max_value_length = 12;  // Assuming maximum integer value length is 10
    int string_length =0;
    if(value != 0)
        string_length = 3 + 1 + strlen(argv[2]) + max_value_length + 2;  // Delimiters, colon, and null terminator
    else
        string_length = 3 + 1 + max_value_length + 2;  // Delimiters, colon, and null terminator
    char formatted_string[string_length];

    // Format the string using sprintf
    if (value == 0)
        snprintf(formatted_string, string_length, ":%c:", delimiter);
    else
        snprintf(formatted_string, string_length, ":%c:%d:", delimiter, value);

    printf("Formatted string: %s\n", formatted_string);
    printf("length/max length = %li/%i\n", strlen(formatted_string), string_length);

    // Create a socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);  // TCP socket
    if (sockfd < 0) {
    perror("socket");
    exit(1);
    }

    // Set up server address information
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    server_addr.sin_port = htons(SERVER_PORT);

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    perror("connect");
    close(sockfd);
    exit(1);
    }

    // Send data to the server
    int bytes_sent = send(sockfd, formatted_string, strlen(formatted_string), 0);
    if (bytes_sent < 0) {
    perror("send");
    close(sockfd);
    exit(1);
    }

    printf("Sent %d bytes of data to the server.\n", bytes_sent);

    // Close the socket
    close(sockfd);

    return 0;
}
