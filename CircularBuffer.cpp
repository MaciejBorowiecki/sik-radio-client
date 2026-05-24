#include <cstddef>
#include <cstring>

#include "CircularBuffer.h"

CircularBuffer::CircularBuffer(size_t size)
    :  buffer(size), head(0), tail(0), current_size(0), capacity(size) {}

size_t CircularBuffer::write(const void *data, size_t len) {
  // Cast to uint8_t* to allow pointer arithmetic.
  const uint8_t *byte_data = static_cast<const uint8_t *>(data);

  size_t bytes_to_write = std::min(this->size_writeable(), len);
  if (bytes_to_write == 0) {
    return 0;
  }

  size_t head_to_end = std::min(capacity - head, bytes_to_write);

  std::memcpy(&buffer[head], byte_data, head_to_end);

  size_t remaining = bytes_to_write - head_to_end;
  if (remaining > 0) {
    std::memcpy(&buffer[0], byte_data + head_to_end, remaining);
  }

  head = (head + bytes_to_write) % capacity;
  current_size += bytes_to_write;

  return bytes_to_write;
}

size_t CircularBuffer::read(void *dest, size_t len) {
  // Cast to uint8_t* to allow pointer arithmetic.
  uint8_t *byte_dest = static_cast<uint8_t *>(dest);

  size_t bytes_to_read = std::min(len, this->size_readable());
  if (bytes_to_read == 0) {
    return 0;
  }

  size_t tail_to_end = std::min(capacity - tail, bytes_to_read);

  std::memcpy(byte_dest, &buffer[tail], tail_to_end);

  size_t remaining = bytes_to_read - tail_to_end;
  if (remaining > 0) {
    std::memcpy((byte_dest + tail_to_end), &buffer[0], remaining);
  }

  tail = (tail + bytes_to_read) % capacity;
  current_size -= bytes_to_read;

  return bytes_to_read;
}

size_t CircularBuffer::size_readable() const { return current_size; }

size_t CircularBuffer::size_writeable() const {
  return capacity - current_size;
}

bool CircularBuffer::read_line(std::string &line) {
  size_t available = this->size_readable();
  size_t line_length = 0;
  bool newline_found = false;

  for (size_t i = 0; i < available; i++) {
    size_t real_index = (tail + i) % capacity;
    line_length++;
    if (buffer[real_index] == '\n') {
      newline_found = true;
      break;
    }
  }

  if (!newline_found) {
    return false;
  }

  // Use CircularBuffer::read to copy data to `line` as it uses memcpy and
  // moves `tail` pointer.
  line.resize(line_length);
  this->read(&(line[0]), line_length);

  // We look for the first occurrence of the newline character so we need to
  // do the following operations only once.
  if (!line.empty() && line.back() == '\n') {
    line.pop_back();
  }
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }

  return true;
}

void CircularBuffer::clear() {
  head = 0;
  tail = 0;
  current_size = 0;
}
