#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include <stddef.h>

#define PACKET_SIZE 1024
#define MAX_PACKETS 100

#define MQ_IMG       "/mq_img"
#define MQ_CHECKSUM  "/mq_checksum"
#define MQ_RESULT    "/mq_result"

typedef struct {
    int id;
    int total_packets;
    size_t data_len;
    uint8_t data[PACKET_SIZE];
} packet_t;

typedef struct {
    uint32_t checksum;
} checksum_t;

typedef struct {
    char status[32];  // "OK" or "CORRUPT"
} result_t;

uint32_t simple_checksum(const uint8_t *data, size_t len);

#endif
