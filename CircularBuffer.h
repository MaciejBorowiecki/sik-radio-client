#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <cstdint>
#include <string>
#include <vector>

class CircularBuffer {
public:
  // Creates a circular buffer of a fixed `size`.
  // Throws `std::bad_alloc` if memory allocation fails.
  CircularBuffer(size_t size);

  // Copies up to `len` bytes from `data` into the buffer.
  // Returns the number of ybtes successfully copied.
  size_t write(const void *data, size_t len);

  // Copies up to `len` bytes from the buffer into `dest`.
  // Returns the number of bytes successfully copied.
  size_t read(void *dest, size_t len);

  // Returns the number of bytes currently available to read.
  size_t size_readable() const;

  // Returns the number of bytes currently available for writing.
  size_t size_writeable() const;

  // Finds and copies the first line ending with newline to the `line` argument.
  // In case of no such line returns `false`.
  bool read_line(std::string &line);

private:
  std::vector<uint8_t> buffer;
  size_t head;
  size_t tail;
  size_t current_size;
  size_t capacity;
};

#endif
