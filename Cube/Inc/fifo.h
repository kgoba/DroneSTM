#pragma once

#include <stdint.h>

#define FIFO(name, type, size) \
                               \
volatile static struct {                \
  type buffer[size];           \
  uint16_t head;                   \
  uint16_t count;                  \
} name;

#define FIFO_INIT(name)     name.head = name.count = 0
#define FIFO_SIZE(name)     (sizeof(name.buffer) / sizeof(name.buffer[0]))
#define FIFO_EMPTY(name)    (name.count == 0)
#define FIFO_FULL(name)     (name.count == FIFO_SIZE(name))
#define FIFO_COUNT(name)    (name.count)

#define FIFO_PUSH(name, b)     \
if (!FIFO_FULL(name)) {         \
  uint16_t tail = name.head + name.count;   \
  if (tail >= FIFO_SIZE(name)) tail -= FIFO_SIZE(name);   \
  name.buffer[tail] = b;    \
  name.count++;             \
}

#define FIFO_HEAD(name)      ((name.count > 0) ? name.buffer[name.head] : 0)
#define FIFO_POP(name, b)     \
if (name.count > 0) {         \
  b = name.buffer[name.head]; \
  name.head++;                \
  if (name.head == FIFO_SIZE(name)) name.head = 0;      \
  name.count--;               \
}

