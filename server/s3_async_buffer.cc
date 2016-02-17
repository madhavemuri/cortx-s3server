/*
 * COPYRIGHT 2015 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.seagate.com/contact
 *
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 3-Dec-2015
 */

#include "s3_async_buffer.h"

S3AsyncBufferContainer::S3AsyncBufferContainer() : buffered_input_length(0), is_expecting_more(true), count_bufs_shared_for_read(0) {
  printf("Created S3AsyncBufferContainer object.\n");
}

// Call this to indicate that no more data will be added to buffer.
void S3AsyncBufferContainer::freeze() {
  is_expecting_more = false;
}

bool S3AsyncBufferContainer::is_freezed() {
  return !is_expecting_more;
}

size_t S3AsyncBufferContainer::length() {
  return buffered_input_length;
}

void S3AsyncBufferContainer::add_content(evbuf_t* buf) {
  printf("S3AsyncBufferContainer::add_content with len %zu at address %p\n", evbuffer_get_length(buf), buf);
  buffered_input.push_back(buf);
  buffered_input_length += evbuffer_get_length(buf);
}

// Call this to get at least expected_content_size of data buffers.
// Anything less is returned only if there is no more data to be filled in.
// Call mark_size_of_data_consumed() to drain the data that was consumed
// after get_buffers_ref call.
// expected_content_size = -1 and all data is available, returns all buffers
std::deque< std::tuple<void*, size_t> >
S3AsyncBufferContainer::get_buffers_ref(size_t expected_content_size) {
  printf("Called S3AsyncBufferContainer::get_buffers_ref with expected_content_size = %zu\n", expected_content_size);

  std::deque< std::tuple<void*, size_t> > data_items;
  size_t len_in_buf = 0;
  size_t num_of_extents = 0;
  struct evbuffer_iovec *vec_in = NULL;
  size_t size_we_can_share = length();
  evbuf_t* buf = NULL;
  count_bufs_shared_for_read = 0;

  if (size_we_can_share >= expected_content_size || !(is_expecting_more)) {
    std::deque<evbuf_t *>::iterator it = buffered_input.begin();
    while(it != buffered_input.end()) {
      buf = *it++;

      bool is_shared_for_read = false;
      len_in_buf = evbuffer_get_length(buf);

      num_of_extents = evbuffer_peek(buf, len_in_buf, NULL, NULL, 0);

      /* do the actual peek */
      vec_in = (struct evbuffer_iovec *)malloc(num_of_extents * sizeof(struct evbuffer_iovec));

      /* do the actual peek at data */
      evbuffer_peek(buf, len_in_buf, NULL/*start of buffer*/, vec_in, num_of_extents);

      for (size_t i = 0; i < num_of_extents; i++) {
        if ((expected_content_size > 0) &&
            ((size_we_can_share >= expected_content_size) || !(is_expecting_more))) {
          if (expected_content_size >= vec_in[i].iov_len) {
            data_items.push_back(std::make_tuple(vec_in[i].iov_base, vec_in[i].iov_len));
          } else {
            data_items.push_back(std::make_tuple(vec_in[i].iov_base, expected_content_size));
          }

          if (expected_content_size >= vec_in[i].iov_len) {
            expected_content_size -= vec_in[i].iov_len;
            size_we_can_share -= vec_in[i].iov_len;
          } else {
            size_we_can_share -= expected_content_size;
            expected_content_size = 0;
          }

          // remember what we are passing out.
          if (!is_shared_for_read) {
            count_bufs_shared_for_read++;
            is_shared_for_read = true;
          }
        } else {
          // We are expecting more data to be filled and we have shared enough,
          // caller can come back later.
          free(vec_in);
          return data_items;
        }
      }
      free(vec_in);
    }
  }
  return data_items;
}

void S3AsyncBufferContainer::mark_size_of_data_consumed(size_t size_consumed) {
  printf("S3AsyncBufferContainer::mark_size_of_data_consumed size_consumed = %zu\n", size_consumed);

  for (size_t i = 0; (i < count_bufs_shared_for_read) && (buffered_input.size() != 0); ++i) {
    if (size_consumed > 0) {
      evbuf_t* buf = buffered_input.front();
      size_t len = evbuffer_get_length(buf);
      if (size_consumed >= len) {
        // Drain everything
        evbuffer_drain(buf, -1);
        size_consumed -= len;
        buffered_input_length -= len;
        buffered_input.pop_front();
        evbuffer_free(buf);
      } else {
        // Partial drain, so we still need to remember this buf
        evbuffer_drain(buf, size_consumed);
        buffered_input_length -= size_consumed;
        size_consumed = 0;
      }
    }
  }
  return;
}

std::string S3AsyncBufferContainer::get_content_as_string() {
  printf("Called S3AsyncBufferContainer::get_content_as_string\n");
  std::string content = "";

  if (is_freezed()) {
    size_t len_in_buf = 0;
    size_t num_of_extents = 0;
    struct evbuffer_iovec *vec_in = NULL;
    evbuf_t* buf = NULL;
    while (!buffered_input.empty()) {
      buf = buffered_input.front();
      buffered_input.pop_front();

      len_in_buf = evbuffer_get_length(buf);

      num_of_extents = evbuffer_peek(buf, len_in_buf, NULL, NULL, 0);

      /* do the actual peek in buf */
      vec_in = (struct evbuffer_iovec *)malloc(num_of_extents * sizeof(struct evbuffer_iovec));

      /* do the actual peek at data inside buf */
      evbuffer_peek(buf, len_in_buf, NULL/*start of buffer*/, vec_in, num_of_extents);

      for (size_t i = 0; i < num_of_extents; i++) {
        content.append((char*)vec_in[i].iov_base, vec_in[i].iov_len);
      }
      free(vec_in);
      evbuffer_free(buf);
    }
  }
  buffered_input_length = 0;  // Everything is returned.
  printf("Return from S3AsyncBufferContainer::get_content_as_string with content size = %zu\n", content.length());

  return content;
}