#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <mqueue.h>
#include <sys/stat.h>
#include <unistd.h>
#include "packet.h"

uint32_t simple_checksum(const uint8_t *data, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++)
        sum += data[i];
    return sum;
}

int main() {
    FILE *img = fopen("cat.jpeg", "rb");
    if (!img) {
        perror("fopen image.jpg");
        return 1;
    }

    // MQ yapılandırmaları
    struct mq_attr attr_img       = {.mq_flags = 0, .mq_maxmsg = 10, .mq_msgsize = sizeof(packet_t)};
    struct mq_attr attr_checksum  = {.mq_flags = 0, .mq_maxmsg = 1,  .mq_msgsize = sizeof(checksum_t)};
    struct mq_attr attr_result    = {.mq_flags = 0, .mq_maxmsg = 1,  .mq_msgsize = sizeof(result_t)};

    // Kuyrukları aç
    mqd_t mq_img = mq_open(MQ_IMG, O_CREAT | O_WRONLY, 0666, &attr_img);
    mqd_t mq_checksum = mq_open(MQ_CHECKSUM, O_CREAT | O_RDONLY, 0666, &attr_checksum);
    mqd_t mq_result = mq_open(MQ_RESULT, O_CREAT | O_WRONLY, 0666, &attr_result);
    if (mq_img == -1 || mq_checksum == -1 || mq_result == -1) {
        perror("mq_open");
        return 1;
    }

    // Paketleme
    packet_t packets[MAX_PACKETS];
    int count = 0;
    uint32_t my_checksum = 0;

    while (!feof(img) && count < MAX_PACKETS) {
        packets[count].id = count;
        packets[count].data_len = fread(packets[count].data, 1, PACKET_SIZE, img);
        packets[count].total_packets = -1;
        my_checksum += simple_checksum(packets[count].data, packets[count].data_len);
        count++;
    }
    fclose(img);

    for (int i = 0; i < count; i++) {
        packets[i].total_packets = count;
        mq_send(mq_img, (char *)&packets[i], sizeof(packet_t), 0);
    }

    // Checksum al
    checksum_t received;
    if (mq_receive(mq_checksum, (char *)&received, sizeof(checksum_t), NULL) == -1) {
        perror("mq_receive checksum");
        return 1;
    }

    result_t res;
    strcpy(res.status, (received.checksum == my_checksum) ? "OK" : "CORRUPT");
    mq_send(mq_result, (char *)&res, sizeof(result_t), 0);

    printf("[ProcessA] My Checksum:       %u\n", my_checksum);
    printf("[ProcessA] Received Checksum: %u\n", received.checksum);
    printf("[ProcessA] Result Sent:       %s\n", res.status);

    mq_close(mq_img);
    mq_close(mq_checksum);
    mq_close(mq_result);
    mq_unlink(MQ_IMG);
    mq_unlink(MQ_CHECKSUM);
    mq_unlink(MQ_RESULT);
    return 0;
}
