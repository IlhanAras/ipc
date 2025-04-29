// circular_queue.h
#ifndef CIRCULAR_QUEUE_H
#define CIRCULAR_QUEUE_H

#include "common.h"
#include <semaphore.h>

typedef struct {
    CircularQueue* queue;
    sem_t* mutex;
    sem_t* count;
} CircularQueueHandle;

// Queue oluştur ve bağlan
int circular_queue_init(const char* shm_name, const char* sem_mutex_name, const char* sem_count_name, CircularQueueHandle* handle, int create);

// Queue kapat
void circular_queue_close(CircularQueueHandle* handle);

// Mesaj ekle (enqueue)
int circular_queue_enqueue(CircularQueueHandle* handle, const Message* msg);

// Mesaj al (dequeue)
int circular_queue_dequeue(CircularQueueHandle* handle, Message* msg);

#endif // CIRCULAR_QUEUE_H
