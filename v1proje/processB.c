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
    // MQ yapılandırmaları
    struct mq_attr attr_img       = {.mq_flags = 0, .mq_maxmsg = 10, .mq_msgsize = sizeof(packet_t)};
    struct mq_attr attr_checksum  = {.mq_flags = 0, .mq_maxmsg = 1,  .mq_msgsize = sizeof(checksum_t)};
    struct mq_attr attr_result    = {.mq_flags = 0, .mq_maxmsg = 1,  .mq_msgsize = sizeof(result_t)};

    mqd_t mq_img = mq_open(MQ_IMG, O_CREAT | O_RDONLY, 0666, &attr_img);
    mqd_t mq_checksum = mq_open(MQ_CHECKSUM, O_CREAT | O_WRONLY, 0666, &attr_checksum);
    mqd_t mq_result = mq_open(MQ_RESULT, O_CREAT | O_RDONLY, 0666, &attr_result);
    if (mq_img == -1 || mq_checksum == -1 || mq_result == -1) {
        perror("mq_open");
        return 1;
    }

    printf("[ProcessB] Waiting for image packets...\n");

    packet_t packets[MAX_PACKETS];
    int total = -1, received = 0;
    uint32_t checksum = 0;

    while (received < MAX_PACKETS) {
        packet_t p;
        if (mq_receive(mq_img, (char *)&p, sizeof(packet_t), NULL) == -1) {
            perror("mq_receive packet");
            continue;
        }

        packets[p.id] = p;
        checksum += simple_checksum(p.data, p.data_len);
        total = p.total_packets;
        received++;

        if (received == total) break;
    }

    printf("[ProcessB] Received %d packets. Reconstructing image...\n", total);

    FILE *out = fopen("reconstructed.jpg", "wb");
    if (!out) {
        perror("fopen");
        return 1;
    }

    for (int i = 0; i < total; i++) {
        fwrite(packets[i].data, 1, packets[i].data_len, out);
    }
    fclose(out);

    checksum_t cs = {.checksum = checksum};
    mq_send(mq_checksum, (char *)&cs, sizeof(checksum_t), 0);
    printf("[ProcessB] Sent Checksum: %u\n", checksum);

    result_t result;
    if (mq_receive(mq_result, (char *)&result, sizeof(result_t), NULL) == -1) {
        perror("mq_receive result");
        return 1;
    }

    printf("[ProcessB] Received Result: %s\n", result.status);

    mq_close(mq_img);
    mq_close(mq_checksum);
    mq_close(mq_result);
    return 0;
}
