#ifndef QUEUE_H
#define QUEUE_H

#include <stdint.h>

typedef struct {
    uint8_t foo;
} QueueHandle_t;

void XQueueReset(QueueHandle_t);

#endif
