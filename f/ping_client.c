#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include "ping.h"
#include "sockets.h"

void print_help() {
    printf("Usage: ping_client [-] <destination_host> <message> <socket_application>\n");
    printf(" Runs a ping client which communicates with the MIP daemon on <socket_application> and tries to ping another host on MIP address <destination_host> with message <message>\n");
    printf(" The options are:\n");
    printf("  -h: Displays this information\n");
    printf("  <destination_host>: MIP address of host to ping\n");
    printf("  <message>: Message to send to pinged host\n");
    printf("  <socket_application>: Pathname to socket\n");
}

int main(int argc, char* argv[]) {

    // Handle Arguments
    if (argc > 1 && strcmp(argv[1], "-h") == 0) {
        print_help();
        return 0;
    }
    
    if (argc != 4) {
        printf("Usage: [-h] <destination_host> <message> <socket_application>\n");
        return -1;
    }

    uint8_t destination_host = (uint8_t) atoi(argv[1]);
    char* message = argv[2];
    char* socket_application = argv[3];

    // Create Socket and set timeout
    int client_socket_fd = create_client_ipc_socket(socket_application, SOCK_STREAM);
    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    setsockopt(client_socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*) &tv, sizeof(tv));

    // Send Message
    printf("Pinging...\n");
    send_app_message(client_socket_fd, destination_host, message);

    // Start Clock
    clock_t timer;
    timer = clock();

    // Recieve Response
    struct app_message* ping = recieve_app_message(client_socket_fd);
    if (ping == NULL) {
        printf("timeout\n");
        close(client_socket_fd);
        return -1;
    }
    printf("%s\n", ping->message);
    free(ping);

    // Print Time Elapsed
    timer = clock() - timer;
    double time_elapsed = ((double) timer) / CLOCKS_PER_SEC;
    printf("Recieved response in %f seconds!\n", time_elapsed);

    // Free memory, close sockets
    close(client_socket_fd);

    return 0;
}
