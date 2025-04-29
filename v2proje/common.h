// common.h
#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>

#define MAX_PAYLOAD_SIZE 1024

// Queue isimleri
#define QUEUE_IMAGE_PACKETS "/queue_image_packets"
#define QUEUE_CHECKSUMS     "/queue_checksums"
#define QUEUE_RESULTS       "/queue_results"

typedef enum {
    PACKET_DATA,   // A -> B : Image Packet
    CHECKSUM_DATA, // B -> A : Checksum
    RESULT_DATA    // A -> B : Result (match/mismatch)
} MessageType;

typedef struct {
    MessageType type;
    int id;                // Paket ID veya sadece tekil mesajlar için 0
    size_t size;            // payload boyutu
    char payload[MAX_PAYLOAD_SIZE];  // asıl veri
} Message;

#endif // COMMON_H
