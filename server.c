#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define PORT 12345
#define BUF_SIZE 1024
#define WINDOW_SIZE 4
#define MAX_SEQ 10

typedef struct {
    int received;
    char data[BUF_SIZE];
} FrameBuffer;

int main() {
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    char buffer[BUF_SIZE];
    socklen_t len;
    int base = 0;

    FrameBuffer window[WINDOW_SIZE];
    for (int i = 0; i < WINDOW_SIZE; i++) {
        window[i].received = 0;
    }

    srand(time(NULL));

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Selective Repeat Receiver started...\n");

    while (1) {
        len = sizeof(cliaddr);
        int n = recvfrom(sockfd, buffer, BUF_SIZE, 0, (struct sockaddr *)&cliaddr, &len);
        if (n < 0) {
            perror("recvfrom error");
            continue;
        }
        buffer[n] = '\0';

        // Simulate 10% packet loss
        if ((rand() % 10) < 1) {
            printf("Packet lost (simulated).\n");
            continue;
        }

        int seq_num;
        char msg[BUF_SIZE];
        sscanf(buffer, "%d|%[^\n]", &seq_num, msg);

        if (seq_num >= base && seq_num < base + WINDOW_SIZE) {
            int idx = seq_num - base;
            if (!window[idx].received) {
                strcpy(window[idx].data, msg);
                window[idx].received = 1;
                printf("Received packet seq %d: %s\n", seq_num, msg);
            } else {
                printf("Duplicate packet seq %d received.\n", seq_num);
            }

            // Send ACK for this packet
            char ack_msg[10];
            sprintf(ack_msg, "ACK%d", seq_num);
            sendto(sockfd, ack_msg, strlen(ack_msg), 0, (struct sockaddr *)&cliaddr, len);

            // Deliver in-order frames and slide window
            while (window[0].received) {
                printf("Delivering data: %s\n", window[0].data);
                // Slide window left
                for (int i = 0; i < WINDOW_SIZE -1; i++) {
                    window[i] = window[i+1];
                }
                window[WINDOW_SIZE -1].received = 0;
                base++;
            }
        } else {
            // Packet out of window range - discard but send ACK if valid
            printf("Packet seq %d outside window (base %d). Discarded.\n", seq_num, base);
            // Send ACK if seq is valid but out of window (may happen if retransmitted)
            if (seq_num < base) {
                char ack_msg[10];
                sprintf(ack_msg, "ACK%d", seq_num);
                sendto(sockfd, ack_msg, strlen(ack_msg), 0, (struct sockaddr *)&cliaddr, len);
            }
        }
    }

    close(sockfd);
    return 0;
}
