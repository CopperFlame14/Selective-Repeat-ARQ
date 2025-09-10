#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

#define SERVER_IP "127.0.0.1"
#define PORT 12345
#define BUF_SIZE 1024
#define WINDOW_SIZE 4
#define MAX_SEQ 10

typedef struct {
    int acked;
    char data[BUF_SIZE];
} Frame;

int main() {
    int sockfd;
    struct sockaddr_in servaddr;
    char buffer[BUF_SIZE];
    socklen_t len = sizeof(servaddr);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set recv timeout
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv)) < 0) {
        perror("Error setting socket timeout");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = inet_addr(SERVER_IP);

    char *messages[MAX_SEQ] = {
        "Msg0", "Msg1", "Msg2", "Msg3", "Msg4",
        "Msg5", "Msg6", "Msg7", "Msg8", "Msg9"
    };

    Frame window[WINDOW_SIZE];
    for (int i = 0; i < WINDOW_SIZE; i++) {
        window[i].acked = 1;  // initially all acked so we can send
    }

    int base = 0, next_seq_num = 0;
    int total_msgs = MAX_SEQ;

    while (base < total_msgs) {
        // Send frames in window
        while (next_seq_num < base + WINDOW_SIZE && next_seq_num < total_msgs) {
            int idx = next_seq_num - base;
            if (window[idx].acked) {
                snprintf(buffer, BUF_SIZE, "%d|%s", next_seq_num, messages[next_seq_num]);
                sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&servaddr, len);
                printf("Sent packet seq %d: %s\n", next_seq_num, messages[next_seq_num]);
                strcpy(window[idx].data, messages[next_seq_num]);
                window[idx].acked = 0;
            }
            next_seq_num++;
        }

        // Wait for ACKs
        int n = recvfrom(sockfd, buffer, BUF_SIZE, 0, NULL, NULL);
        if (n < 0) {
            perror("Timeout or recvfrom error. Resending unacked frames...");
            // Retransmit unacked frames in window
            for (int i = 0; i < WINDOW_SIZE; i++) {
                if (!window[i].acked && (base + i) < total_msgs) {
                    snprintf(buffer, BUF_SIZE, "%d|%s", base + i, window[i].data);
                    sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&servaddr, len);
                    printf("Resent packet seq %d: %s\n", base + i, window[i].data);
                }
            }
            continue;
        }

        buffer[n] = '\0';

        int ack_num;
        if (sscanf(buffer, "ACK%d", &ack_num) == 1) {
            printf("Received %s\n", buffer);
            if (ack_num >= base && ack_num < base + WINDOW_SIZE) {
                int idx = ack_num - base;
                window[idx].acked = 1;

                // Slide window forward for consecutive ACKs
                while (window[0].acked && base < total_msgs) {
                    // shift window left
                    for (int i = 0; i < WINDOW_SIZE - 1; i++) {
                        window[i] = window[i+1];
                    }
                    window[WINDOW_SIZE - 1].acked = 1; // mark new slot as acked to allow sending
                    base++;
                }
            }
        } else {
            printf("Invalid ACK received: %s\n", buffer);
        }
    }

    printf("All packets sent and acknowledged.\n");
    close(sockfd);
    return 0;
}
