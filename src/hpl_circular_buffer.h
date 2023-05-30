#pragma once
#include <array>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <utility>
#ifndef _HPL_CIRCULAR_BUFFER_H_

namespace hpl {
class CircularBuffer {
public:
  CircularBuffer(size_t size)
      : size_(size), buffer_(malloc(size_)), read_pos_(0), write_pos_(0) {}
  ~CircularBuffer() {
    if (buffer_ != nullptr) {
      free(buffer_);
    }
    buffer_ = nullptr;
  }
  CircularBuffer(const CircularBuffer &) = delete;
  CircularBuffer &operator=(const CircularBuffer &) = delete;
  CircularBuffer(CircularBuffer &&other) noexcept {
    if (this != &other) {
      size_ = other.size_;
      buffer_ = other.buffer_;
      read_pos_ = other.read_pos_;
      write_pos_ = other.write_pos_;
      other.size_ = 0;
      other.read_pos_ = 0;
      other.write_pos_ = 0;
      other.buffer_ = nullptr;
    }
  }
  CircularBuffer &operator=(CircularBuffer &&other) noexcept {
    if (this != &other) {
      if (buffer_ != nullptr) {
        free(buffer_);
      }
      size_ = other.size_;
      buffer_ = other.buffer_;
      read_pos_ = other.read_pos_;
      write_pos_ = other.write_pos_;
      other.size_ = 0;
      other.read_pos_ = 0;
      other.write_pos_ = 0;
      other.buffer_ = nullptr;
    }
    return *this;
  }

  size_t Capacity() const { return size_ - 1; }

  std::pair<void *, size_t> ContinuousWriteBuffer() {
    if (write_pos_ < read_pos_) {
      return {(char *)buffer_ + write_pos_, read_pos_ - write_pos_ - 1};
    } else {
      return {(char *)buffer_ + write_pos_,
              size_ - write_pos_ - (read_pos_ == 0 ? 1 : 0)};
    }
  }

  bool Extend(size_t new_size) {
    if (new_size <= size_) {
      return false;
    }
    void *new_buffer = malloc(new_size);
    if (new_buffer == nullptr) {
      return false;
    }
    auto data_len = Length();
    if (data_len > 0) {
      if (read_pos_ < write_pos_) {
        memcpy(new_buffer, (char *)buffer_ + read_pos_, data_len);
      } else {
        memcpy(new_buffer, (char *)buffer_ + read_pos_, size_ - read_pos_);
        memcpy((char *)new_buffer + size_ - read_pos_, buffer_, write_pos_);
      }
    }
    free(buffer_);
    buffer_ = new_buffer;
    size_ = new_size;
    read_pos_ = 0;
    write_pos_ = data_len;
    return true;
  }
  size_t FreeSpace() const { return Capacity() - Length(); }

  /// @brief set the write position, no wrap around
  /// @param whence SEEK_SET, SEEK_CUR, SEEK_END, like fseek
  void ResetWritePos(size_t offset, int whence) {
    if (whence == SEEK_SET) {
      write_pos_ = offset;
    } else if (whence == SEEK_CUR) {
      write_pos_ += offset;
    } else if (whence == SEEK_END) {
      write_pos_ = size_ - offset;
    }
  }

  typedef std::pair<char *, size_t> ReadView;
  std::array<ReadView, 2> GetReadViews() const {
    char *first_read_pos = (char *)buffer_ + read_pos_;
    if (write_pos_ < read_pos_) {
      return {ReadView{first_read_pos, size_ - read_pos_},
              ReadView{(char *)buffer_, write_pos_}};
    } else {
      return {ReadView{first_read_pos, write_pos_ - read_pos_},
              ReadView{nullptr, 0}};
    }
  }

  size_t Popout(size_t len) {
    if (len > Length()) {
      return 0;
    }
    read_pos_ = (read_pos_ + len) % size_;
    return len;
  }

  /// @brief return the current data length in the buffer
  size_t Length() const {
    if (write_pos_ == read_pos_) {
      return 0;
    }
    if (write_pos_ >= read_pos_) {
      return write_pos_ - read_pos_;
    } else {
      return size_ - read_pos_ + write_pos_;
    }
  }

private:
  size_t size_ = 0;
  void *buffer_ = nullptr;

  size_t read_pos_ = 0;
  size_t write_pos_ = 0;
};
} // namespace hpl
#endif // _HPL_CIRCULAR_BUFFER_H_