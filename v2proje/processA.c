// processA.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <mqueue.h>
#include <unistd.h>
#include <sys/stat.h>
#include "common.h"

mqd_t mq_image_packets;
mqd_t mq_checksums;
mqd_t mq_results;
unsigned int real_checksum = 0;

unsigned int simple_checksum(const char* data, size_t size) {
    unsigned int checksum = 0;
    for (size_t i = 0; i < size; ++i)
        checksum += (unsigned char)data[i];
    return checksum;
}

void* sender_thread_func(void* arg) {
    printf("[A-Sender] Started\n");

    FILE* fp = fopen("cat.jpeg", "rb");
    if (!fp) {
        perror("[A-Sender] Failed to open image file");
        pthread_exit(NULL);
    }

    // Dosya boyutunu öğren
    fseek(fp, 0, SEEK_END);
    size_t filesize = ftell(fp);
    rewind(fp);

    // Image'ın checksum'unu hesapla
    char* buffer = malloc(filesize);
    fread(buffer, 1, filesize, fp);
    real_checksum = simple_checksum(buffer, filesize);
    rewind(fp); // başa sar

    printf("[A-Sender] Real checksum calculated: %u\n", real_checksum);

    int packet_id = 0;
    size_t bytes_read;
    Message msg;

    while ((bytes_read = fread(msg.payload, 1, MAX_PAYLOAD_SIZE, fp)) > 0) {
        msg.type = PACKET_DATA;
        msg.id = packet_id++;
        msg.size = bytes_read;

        if (mq_send(mq_image_packets, (char*)&msg, sizeof(msg), 0) == -1) {
            perror("[A-Sender] mq_send failed");
        } else {
            printf("[A-Sender] Sent packet id=%d, size=%zu\n", msg.id, msg.size);
        }
        usleep(10000); // 10 ms bekleme
    }

    free(buffer);
    fclose(fp);

    printf("[A-Sender] Finished sending all packets\n");
    return NULL;
}

void* receiver_thread_func(void* arg) {
    printf("[A-Receiver] Started\n");

    while (1) {
        Message msg;
        ssize_t bytes_read = mq_receive(mq_checksums, (char*)&msg, sizeof(msg), NULL);
        if (bytes_read >= 0) {
            if (msg.type == CHECKSUM_DATA) {
                unsigned int received_checksum;
                memcpy(&received_checksum, msg.payload, sizeof(received_checksum));

                printf("[A-Receiver] Received checksum: %u\n", received_checksum);

                // Gerçek checksum ile karşılaştır
                int match = (received_checksum == real_checksum);

                Message result_msg;
                result_msg.type = RESULT_DATA;
                result_msg.id = 0;
                snprintf(result_msg.payload, sizeof(result_msg.payload), "%s", match ? "MATCH" : "MISMATCH");
                result_msg.size = strlen(result_msg.payload) + 1;

                if (mq_send(mq_results, (char*)&result_msg, sizeof(result_msg), 0) == -1) {
                    perror("[A-Receiver] mq_send (result) failed");
                } else {
                    printf("[A-Receiver] Sent result: %s\n", result_msg.payload);
                }
                break; // İşimiz bitti
            } else {
                printf("[A-Receiver] Unexpected message type, discarded.\n");
            }
        } else {
            usleep(10000); // boşsa uyut
        }
    }

    return NULL;
}

int main() {
    struct mq_attr attr = {
        .mq_flags = 0,
        .mq_maxmsg = 10,
        .mq_msgsize = sizeof(Message),
        .mq_curmsgs = 0
    };

    mq_image_packets = mq_open(QUEUE_IMAGE_PACKETS, O_CREAT | O_WRONLY, 0644, &attr);
    mq_checksums = mq_open(QUEUE_CHECKSUMS, O_CREAT | O_RDONLY | O_NONBLOCK, 0644, &attr);
    mq_results = mq_open(QUEUE_RESULTS, O_CREAT | O_WRONLY, 0644, &attr);

    if (mq_image_packets == -1 || mq_checksums == -1 || mq_results == -1) {
        perror("mq_open error");
        exit(EXIT_FAILURE);
    }

    pthread_t sender_thread, receiver_thread;
    pthread_create(&sender_thread, NULL, sender_thread_func, NULL);
    pthread_create(&receiver_thread, NULL, receiver_thread_func, NULL);

    pthread_join(sender_thread, NULL);
    pthread_join(receiver_thread, NULL);

    mq_close(mq_image_packets);
    mq_close(mq_checksums);
    mq_close(mq_results);

    return 0;
}
