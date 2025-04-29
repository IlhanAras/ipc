// circular_queue.c
#include "circular_queue.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define MAX_RETRY 50     // Maksimum deneme sayısı (5 saniyeye kadar)
#define RETRY_DELAY_US 100000  // 100ms

// Retry destekli circular queue başlatıcı
int circular_queue_init(const char* shm_name, const char* sem_mutex_name, const char* sem_count_name, CircularQueueHandle* handle, int create) {
    int shm_fd;
    int flags = O_RDWR;
    if (create) flags |= O_CREAT;

    // Shared memory retry loop
    for (int attempt = 0; attempt < MAX_RETRY; ++attempt) {
        shm_fd = shm_open(shm_name, flags, 0666);
        if (shm_fd != -1) break;
        if (!create && errno == ENOENT) {
            usleep(RETRY_DELAY_US); // Retry (diğer process henüz yaratmamış olabilir)
            continue;
        }
        perror("shm_open");
        return -1;
    }

    if (create) {
        if (ftruncate(shm_fd, sizeof(CircularQueue)) == -1) {
            perror("ftruncate");
            return -1;
        }
    }

    handle->queue = mmap(NULL, sizeof(CircularQueue), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (handle->queue == MAP_FAILED) {
        perror("mmap");
        return -1;
    }

    if (create) {
        memset(handle->queue, 0, sizeof(CircularQueue));
    }

    int sem_flags = O_RDWR;
    if (create) sem_flags |= O_CREAT;

    // Mutex retry
    for (int attempt = 0; attempt < MAX_RETRY; ++attempt) {
        handle->mutex = sem_open(sem_mutex_name, sem_flags, 0666, 1);
        if (handle->mutex != SEM_FAILED) break;
        if (!create && errno == ENOENT) {
            usleep(RETRY_DELAY_US);
            continue;
        }
        perror("sem_open (mutex)");
        return -1;
    }

    // Count retry
    for (int attempt = 0; attempt < MAX_RETRY; ++attempt) {
        handle->count = sem_open(sem_count_name, sem_flags, 0666, 0);
        if (handle->count != SEM_FAILED) break;
        if (!create && errno == ENOENT) {
            usleep(RETRY_DELAY_US);
            continue;
        }
        perror("sem_open (count)");
        return -1;
    }

    return 0;
}

// Bellek ve semaphor bağlantılarını kapat
void circular_queue_close(CircularQueueHandle* handle) {
    munmap(handle->queue, sizeof(CircularQueue));
    sem_close(handle->mutex);
    sem_close(handle->count);
}

// Enqueue (send)
int circular_queue_enqueue(CircularQueueHandle* handle, const Message* msg) {
    sem_wait(handle->mutex);

    if (handle->queue->count >= MAX_QUEUE_MESSAGES) {
        sem_post(handle->mutex);
        return -1; // Kuyruk dolu
    }

    handle->queue->messages[handle->queue->head] = *msg;
    handle->queue->head = (handle->queue->head + 1) % MAX_QUEUE_MESSAGES;
    handle->queue->count++;

    sem_post(handle->mutex);
    sem_post(handle->count); // Bir mesaj geldi
    return 0;
}

// Dequeue (receive)
int circular_queue_dequeue(CircularQueueHandle* handle, Message* msg) {
    sem_wait(handle->count);      // Mesaj gelene kadar bekle
    sem_wait(handle->mutex);      // Erişim için kilit al

    if (handle->queue->count <= 0) {
        sem_post(handle->mutex);
        return -1; // Boş kuyruk (garip bir durum)
    }

    *msg = handle->queue->messages[handle->queue->tail];
    handle->queue->tail = (handle->queue->tail + 1) % MAX_QUEUE_MESSAGES;
    handle->queue->count--;

    sem_post(handle->mutex);
    return 0;
}
