// processA.c - Görev: Image paketlerini gönderir, checksum sonucu geldiğinde sonucu gönderir.
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

volatile int checksum_received = 0;
unsigned int received_checksum = 0;

unsigned int simple_checksum(const char* data, size_t size) {
    unsigned int sum = 0;
    for (size_t i = 0; i < size; ++i) sum += (unsigned char)data[i];
    return sum;
}

// Receiver thread: Sadece Q2'den checksum alır
void* receiver_thread_func(void* arg) {
    Message msg;
    while (1) {
        if (circular_queue_dequeue(&queue_checksums, &msg) == 0 && msg.type == CHECKSUM_DATA) {
            memcpy(&received_checksum, msg.payload, sizeof(received_checksum));
            checksum_received = 1;
            printf("[A-Receiver] Received checksum: %u\n", received_checksum);
            break;
        }
    }
    return NULL;
}

// Sender thread: Image gönderir ve sonradan sonucu gönderir
void* sender_thread_func(void* arg) {
    FILE* fp;
    while (!(fp = fopen("cat.jpeg", "rb"))) {
        printf("[A-Sender] Waiting for image.bin...\n");
        usleep(500000); // 0.5 saniye bekle
    }

    fseek(fp, 0, SEEK_END);
    size_t filesize = ftell(fp);
    rewind(fp);

    char* buffer = malloc(filesize);
    fread(buffer, 1, filesize, fp);
    unsigned int real_checksum = simple_checksum(buffer, filesize);
    printf("[A-Sender] Real checksum: %u\n", real_checksum);

    int packet_no = 0;
    size_t bytes_read;
    Message msg;
    while ((bytes_read = fread(msg.payload, 1, MAX_PAYLOAD_SIZE, fp)) > 0) {
        msg.type = PACKET_DATA;
        msg.packet_no = packet_no++;
        msg.is_last_packet = (ftell(fp) == filesize);
        msg.size = bytes_read;
        while (circular_queue_enqueue(&queue_packets, &msg) != 0) usleep(1000);
        printf("[A-Sender] Sent packet #%d\n", msg.packet_no);
    }

    free(buffer);
    fclose(fp);

    while (!checksum_received) usleep(1000); // checksum gelene kadar bekle

    Message result_msg;
    result_msg.type = RESULT_DATA;
    result_msg.packet_no = 0;
    result_msg.is_last_packet = 1;
    snprintf(result_msg.payload, sizeof(result_msg.payload), "%s",
             (received_checksum == real_checksum) ? "MATCH" : "MISMATCH");
    result_msg.size = strlen(result_msg.payload) + 1;
    while (circular_queue_enqueue(&queue_results, &result_msg) != 0) usleep(1000);
    printf("[A-Sender] Sent result: %s\n", result_msg.payload);
    return NULL;
}

int main() {
    printf("[A] Starting Process A\n");

    circular_queue_init(SHM_QUEUE_PACKETS, SEM_QUEUE_PACKETS_MUTEX, SEM_QUEUE_PACKETS_COUNT, &queue_packets, 1);
    circular_queue_init(SHM_QUEUE_CHECKSUMS, SEM_QUEUE_CHECKSUMS_MUTEX, SEM_QUEUE_CHECKSUMS_COUNT, &queue_checksums, 1);
    circular_queue_init(SHM_QUEUE_RESULTS, SEM_QUEUE_RESULTS_MUTEX, SEM_QUEUE_RESULTS_COUNT, &queue_results, 1);

    pthread_t sender_thread, receiver_thread;
    pthread_create(&receiver_thread, NULL, receiver_thread_func, NULL);
    pthread_create(&sender_thread, NULL, sender_thread_func, NULL);

    pthread_join(sender_thread, NULL);
    pthread_join(receiver_thread, NULL);

    circular_queue_close(&queue_packets);
    circular_queue_close(&queue_checksums);
    circular_queue_close(&queue_results);

    printf("[A] Finished\n");
    return 0;
}
