#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void sendErrorPacket(int errorCode, int sockfd, struct sockaddr_in client){
    char error_packet[200];
    error_packet[0] = 0;
    error_packet[1] = 5;
    error_packet[2] = 0;
    error_packet[3] = errorCode;
    switch(errorCode){
        case 0:
            strcpy(error_packet+4, "Undefined error");
            break;
        case 1:
            strcpy(error_packet+4, "File or directory not found");
            break;
        case 2:
            strcpy(error_packet+4,  "Access violation");
            break;
        case 3:
            strcpy(error_packet+4, "Disk full or allocation exceeded");
            break;
        case 4:
            strcpy(error_packet+4, "Illegal TFTP operation");
            break;
        case 5:
            strcpy(error_packet+4, "Unknown transfer ID");
            break;
        case 6:
            strcpy(error_packet+4, "File already exists");
            break;
        case 7:
            strcpy(error_packet+4, "No such user");
            break;
    }
    sendto(sockfd, error_packet, sizeof(error_packet), 0, (struct sockaddr *) &client,  sizeof(client));
}

void sendPacket(int opCode, int sockfd, int blockcount, char* file_buffer, int size, struct sockaddr_in client){
    char packet_buffer[516];
    packet_buffer[0] = 0;
    packet_buffer[1] = opCode;
    packet_buffer[2] = (blockcount >> 8) & 0xFF;
    packet_buffer[3] = (blockcount)&0xFF;
    memcpy(packet_buffer+4, file_buffer, size);
    sendto(sockfd, packet_buffer, size+4, 0, (struct sockaddr *) &client,  sizeof(client));
}

void serve_client(int sockfd, char *filename, struct sockaddr_in client)
{
    char file_buffer[512];
    char reply_buffer[512];
    unsigned short blockcount = 0;
    FILE *fp;
    if((fp = fopen(filename, "r")) == NULL){
        sendErrorPacket(1, sockfd, client);
        exit(1);
    }
    socklen_t len = (socklen_t) sizeof(client);
    memset(file_buffer, 0, sizeof(file_buffer));
    int size;
    do{
        blockcount++;
        size = fread(file_buffer, 1, 512, fp);
        //Send data packet
        sendPacket(3, sockfd, blockcount, file_buffer, size, client);
        memset(reply_buffer, 0, sizeof(reply_buffer));
        //Wait for acknowledgement packet
        recvfrom(sockfd, reply_buffer, sizeof(reply_buffer)-1, 0, (struct sockaddr *) &client, &len);
        if(reply_buffer[1] != 4){
            /*send error package if the client doesn't reply with an acknowledgement packet
            something has gone wrong*/
            sendErrorPacket(4, sockfd, client);
            exit(1);
        }
    }
    while(size == 512);
    printf("file delivered\n");
    fclose(fp);
}

int main(int argc, char *argv[])
{
    // 2 arguments have to be entered for correct execution
    if ( argc != 3 )
    {
        printf( "Please insert arguments PORT and DATA DIRECTORY to execute %s", argv[0] );
	    exit(1);
    }
    if((chdir(argv[2])) == -1){
        printf("Directory not found!");
        exit(1);
    }
	unsigned short port = atoi(argv[1]);
    int sockfd;
    struct sockaddr_in server, client;
    char message[512];
    char filename[255];
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&server, 0, sizeof(server));

    //IPv4 address
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);
    bind(sockfd, (struct sockaddr *) &server, (socklen_t) sizeof(server));
    while(1){
        memset(filename, 0, sizeof(filename));
        memset(message, 0, sizeof(message));
        socklen_t len = (socklen_t) sizeof(client);
        //client address gets filled in by the recvfrom request
        recvfrom(sockfd, message, sizeof(message)-1, 0, (struct sockaddr *) &client, &len);
        //serve the client if it's a RRQ.
        if(message[1] != 1){
            sendErrorPacket(4, sockfd, client);
            continue;
        }
        else{
            //the amateur way of retrieving the filename from the read request
            int index = 2;
            while(message[index] != '\0'){
                strncat(filename, &message[index], 1);
                index++;
            }
            char *ip = inet_ntoa(client.sin_addr);
            printf("file \"%s\" requested from %s:%s\n", filename, ip, argv[1]);
            serve_client(sockfd, filename, client);
        }
        fflush(stdout);
    }
    return 0;
}
