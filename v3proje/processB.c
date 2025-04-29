// processB.c
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

char* reconstructed_image = NULL;
size_t reconstructed_size = 0;
size_t reconstructed_capacity = 0;

unsigned int simple_checksum(const char* data, size_t size) {
    unsigned int checksum = 0;
    for (size_t i = 0; i < size; ++i) {
        checksum += (unsigned char)data[i];
    }
    return checksum;
}

void* receiver_thread_func(void* arg) {
    printf("[B-Receiver] Started\n");

    int last_packet_received = 0;

    while (!last_packet_received) {
        Message msg;
        if (circular_queue_dequeue(&queue_packets, &msg) == 0) {
            if (msg.type == PACKET_DATA) {
                printf("[B-Receiver] Received packet %d (last=%d)\n", msg.packet_no, msg.is_last_packet);

                // Büyütme gerekirse
                if (reconstructed_capacity < (msg.packet_no + 1) * MAX_PAYLOAD_SIZE) {
                    reconstructed_capacity += 10 * MAX_PAYLOAD_SIZE;
                    reconstructed_image = realloc(reconstructed_image, reconstructed_capacity);
                }

                memcpy(reconstructed_image + (msg.packet_no * MAX_PAYLOAD_SIZE), msg.payload, msg.size);
                reconstructed_size += msg.size;

                if (msg.is_last_packet) {
                    last_packet_received = 1;
                }
            }
        }
    }

    printf("[B-Receiver] All packets received.\n");
    return NULL;
}

void* sender_thread_func(void* arg) {
    printf("[B-Sender] Started\n");

    // receiver_thread bitene kadar biraz bekle (senkronizasyon için join kullanacağız)
    pthread_join(*(pthread_t*)arg, NULL);

    unsigned int checksum = simple_checksum(reconstructed_image, reconstructed_size);
    printf("[B-Sender] Calculated checksum: %u\n", checksum);

    Message checksum_msg;
    checksum_msg.type = CHECKSUM_DATA;
    checksum_msg.packet_no = 0;
    checksum_msg.is_last_packet = 1;
    memcpy(checksum_msg.payload, &checksum, sizeof(checksum));
    checksum_msg.size = sizeof(checksum);

    circular_queue_enqueue(&queue_checksums, &checksum_msg);

    // Result bekliyoruz
    Message result_msg;
    if (circular_queue_dequeue(&queue_results, &result_msg) == 0) {
        if (result_msg.type == RESULT_DATA) {
            printf("[B-Sender] Received result: %s\n", result_msg.payload);
        }
    }

    return NULL;
}

int main() {
    printf("[B] Starting\n");

    if (circular_queue_init(SHM_QUEUE_PACKETS, SEM_QUEUE_PACKETS_MUTEX, SEM_QUEUE_PACKETS_COUNT, &queue_packets, 0) != 0 ||
        circular_queue_init(SHM_QUEUE_CHECKSUMS, SEM_QUEUE_CHECKSUMS_MUTEX, SEM_QUEUE_CHECKSUMS_COUNT, &queue_checksums, 0) != 0 ||
        circular_queue_init(SHM_QUEUE_RESULTS, SEM_QUEUE_RESULTS_MUTEX, SEM_QUEUE_RESULTS_COUNT, &queue_results, 0) != 0) {
        printf("[B] Failed to initialize queues\n");
        exit(EXIT_FAILURE);
    }

    pthread_t receiver_thread, sender_thread;
    pthread_create(&receiver_thread, NULL, receiver_thread_func, NULL);
    pthread_create(&sender_thread, NULL, sender_thread_func, &receiver_thread);

    pthread_join(sender_thread, NULL);

    circular_queue_close(&queue_packets);
    circular_queue_close(&queue_checksums);
    circular_queue_close(&queue_results);

    if (reconstructed_image)
        free(reconstructed_image);

    printf("[B] Finished\n");
    return 0;
}
