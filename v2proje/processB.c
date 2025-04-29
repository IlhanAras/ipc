// processB.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <mqueue.h>
#include <unistd.h>
#include "common.h"

mqd_t mq_image_packets;
mqd_t mq_checksums;
mqd_t mq_results;

char* image_data = NULL;
size_t total_received_size = 0;
size_t buffer_capacity = 0;

unsigned int simple_checksum(const char* data, size_t size) {
    unsigned int checksum = 0;
    for (size_t i = 0; i < size; ++i)
        checksum += (unsigned char)data[i];
    return checksum;
}

void* receiver_thread_func(void* arg) {
    printf("[B-Receiver] Started\n");

    while (1) {
        Message msg;
        ssize_t bytes_read = mq_receive(mq_image_packets, (char*)&msg, sizeof(msg), NULL);
        if (bytes_read >= 0) {
            if (msg.type == PACKET_DATA) {
                printf("[B-Receiver] Received packet id=%d, size=%zu\n", msg.id, msg.size);

                if (total_received_size + msg.size > buffer_capacity) {
                    buffer_capacity += 10 * MAX_PAYLOAD_SIZE; // büyüt
                    image_data = realloc(image_data, buffer_capacity);
                }

                memcpy(image_data + total_received_size, msg.payload, msg.size);
                total_received_size += msg.size;
            } else {
                printf("[B-Receiver] Unexpected message type, discarded.\n");
            }
        } else {
            usleep(10000);
        }

        // Burada bir mekanizma ile son paketin geldiğini anlayabiliriz.
        // Şu anlık basit versiyon: belli bir süre boyunca paket gelmezse checksum gönderelim
    }
    return NULL;
}

void* sender_thread_func(void* arg) {
    printf("[B-Sender] Started\n");

    sleep(3); // bekle ki tüm paketler alınsın

    unsigned int checksum = simple_checksum(image_data, total_received_size);
    printf("[B-Sender] Calculated checksum: %u\n", checksum);

    Message checksum_msg;
    checksum_msg.type = CHECKSUM_DATA;
    checksum_msg.id = 0;
    memcpy(checksum_msg.payload, &checksum, sizeof(checksum));
    checksum_msg.size = sizeof(checksum);

    if (mq_send(mq_checksums, (char*)&checksum_msg, sizeof(checksum_msg), 0) == -1) {
        perror("[B-Sender] mq_send (checksum) failed");
    } else {
        printf("[B-Sender] Sent checksum.\n");
    }

    while (1) {
        Message msg;
        ssize_t bytes_read = mq_receive(mq_results, (char*)&msg, sizeof(msg), NULL);
        if (bytes_read >= 0) {
            if (msg.type == RESULT_DATA) {
                printf("[B-Sender] Received result: %s\n", msg.payload);
                break;
            } else {
                printf("[B-Sender] Unexpected message type, discarded.\n");
            }
        } else {
            usleep(10000);
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

    mq_image_packets = mq_open(QUEUE_IMAGE_PACKETS, O_CREAT | O_RDONLY | O_NONBLOCK, 0644, &attr);
    mq_checksums = mq_open(QUEUE_CHECKSUMS, O_CREAT | O_WRONLY, 0644, &attr);
    mq_results = mq_open(QUEUE_RESULTS, O_CREAT | O_RDONLY | O_NONBLOCK, 0644, &attr);

    if (mq_image_packets == -1 || mq_checksums == -1 || mq_results == -1) {
        perror("mq_open error");
        exit(EXIT_FAILURE);
    }

    pthread_t receiver_thread, sender_thread;
    pthread_create(&receiver_thread, NULL, receiver_thread_func, NULL);
    pthread_create(&sender_thread, NULL, sender_thread_func, NULL);

    pthread_join(receiver_thread, NULL);
    pthread_join(sender_thread, NULL);

    mq_close(mq_image_packets);
    mq_close(mq_checksums);
    mq_close(mq_results);

    if (image_data) {
        free(image_data);
    }

    return 0;
}
