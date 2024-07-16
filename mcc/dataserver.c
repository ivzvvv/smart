#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>

#define PORT 5000 
  
// Driver code 
int main() 
{   
    char buffer[100000]; 
    
    FILE *out_data = fopen("downsample.bin", "a");
    int rcvd_msg_data = 0;
    
    int listenfd, len; 
    struct sockaddr_in servaddr, cliaddr; 
    bzero(&servaddr, sizeof(servaddr)); 
    
    // Create a UDP Socket 
    listenfd = socket(AF_INET, SOCK_DGRAM, 0);         
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    servaddr.sin_port = htons(5000); 
    servaddr.sin_family = AF_INET;  
   
    // bind server address to socket descriptor 
    bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)); 
    
    //receive the datagram 
    while(1){
        len = sizeof(cliaddr); 
        int n = recvfrom(listenfd, buffer, sizeof(buffer), 
                0, (struct sockaddr*)&cliaddr,&len); //receive message from server 
        //buffer[n] = '\0'; 
        fwrite(buffer, sizeof(int16_t)*n, 1, out_data);
        printf("Received %i bytes\n", n);
    }
    
    fclose(out_data);
    return 0;
} 
