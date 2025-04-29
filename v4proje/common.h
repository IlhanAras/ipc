// common.h
#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>

// Her mesajın taşıyabileceği maksimum veri boyutu (1 KB)
#define MAX_PAYLOAD_SIZE 1024

// Circular queue'nun alabileceği maksimum mesaj sayısı (100 mesajlık halka)
#define MAX_QUEUE_MESSAGES 100

// 3 farklı shared memory alanı (3 farklı queue)
#define SHM_QUEUE_PACKETS   "/queue_packets"     // A ➜ B : Image packetleri
#define SHM_QUEUE_CHECKSUMS "/queue_checksums"   // B ➜ A : Checksum mesajı
#define SHM_QUEUE_RESULTS   "/queue_results"     // A ➜ B : Karşılaştırma sonucu

// Semaphorlar: her queue için bir mutex (kilit) ve bir count (sinyal)
#define SEM_QUEUE_PACKETS_MUTEX   "/sem_packets_mutex"
#define SEM_QUEUE_PACKETS_COUNT   "/sem_packets_count"

#define SEM_QUEUE_CHECKSUMS_MUTEX "/sem_checksums_mutex"
#define SEM_QUEUE_CHECKSUMS_COUNT "/sem_checksums_count"

#define SEM_QUEUE_RESULTS_MUTEX   "/sem_results_mutex"
#define SEM_QUEUE_RESULTS_COUNT   "/sem_results_count"

// Mesaj türleri
typedef enum {
    PACKET_DATA,    // A ➜ B : Image veri parçası
    CHECKSUM_DATA,  // B ➜ A : Checksum bilgisi
    RESULT_DATA     // A ➜ B : MATCH / MISMATCH sonucu
} MessageType;

// Mesaj veri yapısı (1 paket)
typedef struct {
    MessageType type;               // Mesaj türü (enum)
    int packet_no;                  // Paket sırası (0, 1, 2, ...)
    int is_last_packet;            // Bu paket son paket mi? (1 = evet)
    size_t size;                   // payload içinde kaç byte kullanıldı
    char payload[MAX_PAYLOAD_SIZE]; // Gerçek veri içeriği
} Message;

// Circular Queue (ring buffer) veri yapısı
typedef struct {
    int head;                      // Sonraki yazma konumu
    int tail;                      // Sonraki okuma konumu
    int count;                     // Kuyruktaki mesaj sayısı
    Message messages[MAX_QUEUE_MESSAGES]; // Mesaj dizisi
} CircularQueue;

#endif // COMMON_H
