// common.h
#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>

#define MAX_PAYLOAD_SIZE 1024     // Her paket 1024 byte
#define MAX_QUEUE_MESSAGES 100    // Her circular queue 100 mesaj tutacak

// Shared memory ve semaphore isimleri
#define SHM_QUEUE_PACKETS   "/queue_packets"
#define SEM_QUEUE_PACKETS_MUTEX "/sem_packets_mutex"
#define SEM_QUEUE_PACKETS_COUNT "/sem_packets_count"

#define SHM_QUEUE_CHECKSUMS "/queue_checksums"
#define SEM_QUEUE_CHECKSUMS_MUTEX "/sem_checksums_mutex"
#define SEM_QUEUE_CHECKSUMS_COUNT "/sem_checksums_count"

#define SHM_QUEUE_RESULTS   "/queue_results"
#define SEM_QUEUE_RESULTS_MUTEX "/sem_results_mutex"
#define SEM_QUEUE_RESULTS_COUNT "/sem_results_count"

typedef enum {
    PACKET_DATA,
    CHECKSUM_DATA,
    RESULT_DATA
} MessageType;

typedef struct {
    MessageType type;        // Message tipi (packet, checksum, result)
    int packet_no;           // Paket numarası (0,1,2,..)
    int is_last_packet;      // 1 ise son packet
    size_t size;             // Payload size
    char payload[MAX_PAYLOAD_SIZE];  // Veri
} Message;

// Circular Queue yapısı
typedef struct {
    int head; // Yazma pozisyonu
    int tail; // Okuma pozisyonu
    int count; // İçerideki mesaj sayısı
    Message messages[MAX_QUEUE_MESSAGES];
} CircularQueue;

#endif // COMMON_H
