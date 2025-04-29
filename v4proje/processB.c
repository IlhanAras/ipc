// processB.c - Görev: image paketlerini alır, reconstruct yapar, checksum hesaplayıp gönderir, sonucu alır.
#include "common.h"
#include "circular_queue.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

CircularQueueHandle queue_packets;
CircularQueueHandle queue_checksums;
CircularQueueHandle queue_results;

char* reconstructed_image = NULL;
size_t reconstructed_size = 0;
size_t capacity = 0;

unsigned int simple_checksum(const char* data, size_t size) {
    unsigned int sum = 0;
    for (size_t i = 0; i < size; ++i) sum += (unsigned char)data[i];
    return sum;
}

// Receiver thread: Q1'den image alır, Q3'ten sonucu alır
void* receiver_thread_func(void* arg) {
    int last_packet_received = 0;

    while (!last_packet_received) {
        Message msg;
        if (circular_queue_dequeue(&queue_packets, &msg) == 0 && msg.type == PACKET_DATA) {
            printf("[B-Receiver] Received packet #%d\n", msg.packet_no);
            size_t offset = msg.packet_no * MAX_PAYLOAD_SIZE;
            if (capacity < offset + msg.size) {
                capacity = offset + msg.size + MAX_PAYLOAD_SIZE;
                reconstructed_image = realloc(reconstructed_image, capacity);
            }
            memcpy(reconstructed_image + offset, msg.payload, msg.size);
            reconstructed_size += msg.size;

            if (msg.is_last_packet) {
                last_packet_received = 1;
            }
        }
    }

    printf("[B-Receiver] All packets received.\n");

    Message result_msg;
    if (circular_queue_dequeue(&queue_results, &result_msg) == 0 && result_msg.type == RESULT_DATA) {
        printf("[B-Receiver] Received result: %s\n", result_msg.payload);
    }

    return NULL;
}

// Sender thread: checksum hesaplayıp A’ya yollar
void* sender_thread_func(void* arg) {
    pthread_join(*(pthread_t*)arg, NULL); // receiver bitmeden başlamasın

    unsigned int checksum = simple_checksum(reconstructed_image, reconstructed_size);
    printf("[B-Sender] Calculated checksum: %u\n", checksum);

    Message checksum_msg;
    checksum_msg.type = CHECKSUM_DATA;
    checksum_msg.packet_no = 0;
    checksum_msg.is_last_packet = 1;
    memcpy(checksum_msg.payload, &checksum, sizeof(checksum));
    checksum_msg.size = sizeof(checksum);

    while (circular_queue_enqueue(&queue_checksums, &checksum_msg) != 0)
        usleep(1000);

    return NULL;
}

int main() {
    printf("[B] Starting Process B\n");

    circular_queue_init(SHM_QUEUE_PACKETS, SEM_QUEUE_PACKETS_MUTEX, SEM_QUEUE_PACKETS_COUNT, &queue_packets, 0);
    circular_queue_init(SHM_QUEUE_CHECKSUMS, SEM_QUEUE_CHECKSUMS_MUTEX, SEM_QUEUE_CHECKSUMS_COUNT, &queue_checksums, 0);
    circular_queue_init(SHM_QUEUE_RESULTS, SEM_QUEUE_RESULTS_MUTEX, SEM_QUEUE_RESULTS_COUNT, &queue_results, 0);

    pthread_t receiver_thread, sender_thread;
    pthread_create(&receiver_thread, NULL, receiver_thread_func, NULL);
    pthread_create(&sender_thread, NULL, sender_thread_func, &receiver_thread);

    pthread_join(sender_thread, NULL);

    circular_queue_close(&queue_packets);
    circular_queue_close(&queue_checksums);
    circular_queue_close(&queue_results);

    if (reconstructed_image) free(reconstructed_image);

    printf("[B] Finished\n");
    return 0;
}
