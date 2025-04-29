// processA.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include "common.h"
#include "circular_queue.h"
#include <unistd.h>
#include <sys/stat.h>

CircularQueueHandle queue_packets;
CircularQueueHandle queue_checksums;
CircularQueueHandle queue_results;
unsigned int real_checksum = 0;

unsigned int simple_checksum(const char* data, size_t size) {
    unsigned int checksum = 0;
    for (size_t i = 0; i < size; ++i) {
        checksum += (unsigned char)data[i];
    }
    return checksum;
}

void* sender_thread_func(void* arg) {
    printf("[A-Sender] Started\n");

    FILE* fp = fopen("cat.jpeg", "rb");
    if (!fp) {
        perror("[A-Sender] Failed to open image");
        pthread_exit(NULL);
    }

    fseek(fp, 0, SEEK_END);
    size_t filesize = ftell(fp);
    rewind(fp);

    char* buffer = malloc(filesize);
    fread(buffer, 1, filesize, fp);
    real_checksum = simple_checksum(buffer, filesize);
    rewind(fp);

    printf("[A-Sender] Real checksum: %u\n", real_checksum);

    int packet_no = 0;
    size_t bytes_read;
    Message msg;

    while ((bytes_read = fread(msg.payload, 1, MAX_PAYLOAD_SIZE, fp)) > 0) {
        msg.type = PACKET_DATA;
        msg.packet_no = packet_no++;
        msg.is_last_packet = 0; // Şimdilik değil

        if (ftell(fp) == filesize) {
            msg.is_last_packet = 1; // Bu son packet
        }

        msg.size = bytes_read;

        if (circular_queue_enqueue(&queue_packets, &msg) != 0) {
            printf("[A-Sender] Failed to enqueue packet %d\n", msg.packet_no);
        } else {
            printf("[A-Sender] Sent packet %d\n", msg.packet_no);
        }
    }

    free(buffer);
    fclose(fp);

    return NULL;
}

void* receiver_thread_func(void* arg) {
    printf("[A-Receiver] Started\n");

    Message msg;

    // Checksum bekliyoruz
    if (circular_queue_dequeue(&queue_checksums, &msg) == 0) {
        if (msg.type == CHECKSUM_DATA) {
            unsigned int received_checksum;
            memcpy(&received_checksum, msg.payload, sizeof(received_checksum));

            printf("[A-Receiver] Received checksum: %u\n", received_checksum);

            int match = (received_checksum == real_checksum);

            Message result_msg;
            result_msg.type = RESULT_DATA;
            result_msg.packet_no = 0;
            result_msg.is_last_packet = 1;
            snprintf(result_msg.payload, sizeof(result_msg.payload), "%s", match ? "MATCH" : "MISMATCH");
            result_msg.size = strlen(result_msg.payload) + 1;

            circular_queue_enqueue(&queue_results, &result_msg);

            printf("[A-Receiver] Sent result: %s\n", result_msg.payload);
        }
    }

    return NULL;
}

int main() {
    printf("[A] Starting\n");

    // Queue bağlantılarını kur
    if (circular_queue_init(SHM_QUEUE_PACKETS, SEM_QUEUE_PACKETS_MUTEX, SEM_QUEUE_PACKETS_COUNT, &queue_packets, 1) != 0 ||
        circular_queue_init(SHM_QUEUE_CHECKSUMS, SEM_QUEUE_CHECKSUMS_MUTEX, SEM_QUEUE_CHECKSUMS_COUNT, &queue_checksums, 1) != 0 ||
        circular_queue_init(SHM_QUEUE_RESULTS, SEM_QUEUE_RESULTS_MUTEX, SEM_QUEUE_RESULTS_COUNT, &queue_results, 1) != 0) {
        printf("[A] Failed to initialize queues\n");
        exit(EXIT_FAILURE);
    }

    pthread_t sender_thread, receiver_thread;
    pthread_create(&sender_thread, NULL, sender_thread_func, NULL);
    pthread_create(&receiver_thread, NULL, receiver_thread_func, NULL);

    pthread_join(sender_thread, NULL);
    pthread_join(receiver_thread, NULL);

    circular_queue_close(&queue_packets);
    circular_queue_close(&queue_checksums);
    circular_queue_close(&queue_results);

    printf("[A] Finished\n");
    return 0;
}
