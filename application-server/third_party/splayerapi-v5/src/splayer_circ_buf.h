/*************************************************************************
 *
 * SOUNDTRACK TECHNOLOGIES SWEDEN AB - CONFIDENTIAL
 * __________________
 *
 *  [2025] - SOUNDTRACK TECHNOLOGIES SWEDEN AB
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of SOUNDTRACK TECHNOLOGIES SWEDEN AB and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to SOUNDTRACK TECHNOLOGIES SWEDEN AB
 * and its suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret and copyright law.
 * Dissemination of this information or reproduction, sub-licensing or
 * modification of this material is strictly forbidden unless prior written
 * permission is obtained from SOUNDTRACK TECHNOLOGIES SWEDEN AB.
 * Violations of these rights will result in legal actions.
 */
#pragma once

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Workaround for raspbery pi compiler that fails to include stdint.h */
#ifndef SIZE_MAX
#define SIZE_MAX (4294967295U)
#endif
#define MAX(a, b) ((a >= b) ? a : b)
#define MIN(a, b) ((a <= b) ? a : b)

typedef struct circ_buf {
  size_t size;
  size_t used;
  size_t pos;
  size_t left; // Used by marker.
  char* buf;
} circ_buf_t;

static inline void circ_init(circ_buf_t* buf, char* data, size_t size) {
  memset(buf, 0, sizeof(*buf));
  buf->buf = data;
  buf->size = size;
  buf->left = SIZE_MAX;
}

static inline size_t circ_get_free_bytes(circ_buf_t* buf) {
  return buf->size - buf->used;
}

static inline size_t circ_used(circ_buf_t* buf) {
  return buf->used;
}

static inline size_t circ_left(circ_buf_t* buf) {
  return buf->left;
}

static inline size_t circ_total_size(circ_buf_t* buf) {
  return buf->size;
}

static inline void circ_clear_to_marker(circ_buf_t* buf) {
  if (buf->left != SIZE_MAX) {
    buf->used = MIN(buf->used, buf->left);
    buf->left = SIZE_MAX;
  } else {
    buf->used = 0;
  }
}

static inline void circ_clear(circ_buf_t* buf) {
  buf->used = 0;
  buf->pos = 0;
  buf->left = SIZE_MAX;
}

//
//   Example 1:
//    size: 32
//    used: 10
//    pos:  3
//   |___XXXXXXXXX___________________
//
//   Example 2:
//    size: 32
//    used: 30
//    pos:  11
//   |XXXXXXXXX__XXXXXXXXXXXXXXXXXXXX
//

// Possible push states:
// Push case 1, appending 5 bytes when used: 2,  pos:  4, size: 32.
//   |__XXAAAAA_______________________
// Push case 2, appending 5 bytes when used: 10, pos: 19, size: 32.
//   |BB_________________XXXXXXXXXXAAA
// Push case 3, appending 5 bytes when used: 15, pos: 19, size: 32.
//   |XXAAAAA____________XXXXXXXXXXXXX
//
static inline size_t circ_push(circ_buf_t* buf, const char* data, size_t data_size) {
  data_size = MIN(data_size, circ_get_free_bytes(buf));
  if ((buf->pos + buf->used) > buf->size) {
    // Case 3, pos wraps around.
    memcpy(buf->buf + buf->pos + buf->used - buf->size, data, data_size);
  } else if ((buf->pos + buf->used + data_size) > buf->size) {
    // Handle Case 2, split means no (AAA) bytes.
    size_t split = buf->size - (buf->pos + buf->used);
    memcpy(buf->buf + buf->pos + buf->used, data, split);
    memcpy(buf->buf, data + split, data_size - split);
  } else {
    // Case 1.
    memcpy(buf->buf + buf->pos + buf->used, data, data_size);
  }
  buf->used += data_size;
  return data_size;
}

static inline int circ_push_marker(circ_buf_t* buf) {
  if (buf->left != SIZE_MAX)
    return 0;
  buf->left = buf->used;
  return 1;
}

static inline int circ_pop_marker(circ_buf_t* buf) {
  if (buf->left == SIZE_MAX)
    return 0;

  if (buf->left == 0) {
    buf->left = SIZE_MAX;
    return 1;
  }
  return 0;
}

static inline int circ_is_marked(circ_buf_t* buf) {
  if (buf->left == SIZE_MAX)
    return 0;

  if (buf->left == 0) {
    return 1;
  }
  return 0;
}

static inline void circ_remove_marker(circ_buf_t* buf) {
  buf->left = SIZE_MAX;
}

static inline size_t circ_pull(circ_buf_t* buf, char* data, size_t data_size) {
  data_size = MIN(data_size, buf->used);
  data_size = MIN(data_size, buf->left);

  if ((buf->pos + data_size) >= buf->size) {
    memcpy(data, buf->buf + buf->pos, buf->size - buf->pos);
    memcpy(data + (buf->size - buf->pos), buf->buf, data_size - (buf->size - buf->pos));
    buf->pos = data_size - (buf->size - buf->pos);
  } else {
    memcpy(data, buf->buf + buf->pos, data_size);
    buf->pos += data_size;
  }
  buf->used -= data_size;
  if (buf->left != SIZE_MAX)
    buf->left -= data_size;
  return data_size;
}

static inline size_t circ_pull_ptr(circ_buf_t* buf, char** data) {
  *data = buf->buf + buf->pos;
  return MIN(buf->size - buf->pos, buf->used);
}

static inline void circ_pull_ptr_pulled(circ_buf_t* buf, size_t pulled_bytes) {
  if (pulled_bytes > buf->used) {
    printf("Pulled more bytes than exists in buffer\n");
  }
  buf->pos += pulled_bytes;
  if (buf->pos >= buf->size)
    buf->pos -= buf->size;
  buf->used -= pulled_bytes;
}

static inline size_t circ_push_ptr(circ_buf_t* buf, char** data) {
  size_t pos = buf->pos + buf->used;
  if (pos >= buf->size)
    pos -= buf->size;
  *data = buf->buf + pos;
  // Return the min value of the reset of space to the end of the buffer, or what's free in the buffer.
  return MIN(buf->size - pos, buf->size - buf->used);
}

static inline void circ_push_ptr_pushed(circ_buf_t* buf, size_t pushed_bytes) {
  if ((pushed_bytes + buf->used) > buf->size)
    printf("Pushed more bytes than exists in buffer\n");
  buf->used += pushed_bytes;
}

static inline size_t circ_push_data(circ_buf_t* buf, char* data, size_t size) {
  size_t written = 0;
  if (circ_get_free_bytes(buf) < size)
    return 0;
  while (written < size) {
    char* buf_data = NULL;
    size_t bytes_to_write = circ_push_ptr(buf, &buf_data);
    if (!bytes_to_write)
      return written;

    if ((size - written) < bytes_to_write)
      bytes_to_write = size - written;

    memcpy(buf_data, data + written, bytes_to_write);
    circ_push_ptr_pushed(buf, bytes_to_write);
    written += bytes_to_write;
  }
  return written;
}
