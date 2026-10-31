// An io_uring that sends a 64-bit value to another ring with IORING_OP_MSG_RING and waits for the values that
// other rings send to it. The library uses it to hand work to its threads and to get the answers back.
// https://man7.org/linux/man-pages/man3/io_uring_prep_msg_ring.3.html
#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <system_error>

namespace konsens {

class ring_error : public std::system_error {
public:
    using std::system_error::system_error;
};

class ring {
public:
    [[nodiscard]] static std::expected<ring, std::error_code> with(unsigned entries);

    [[nodiscard]] std::expected<void, std::error_code> msg_ring(const ring& to, std::uint64_t user_data);
    [[nodiscard]] std::expected<std::uint64_t, std::error_code> wait_cqe();

    ring(ring&&) noexcept;
    ring& operator=(ring&&) noexcept;
    ~ring();

private:
    struct queue;

    explicit ring(std::unique_ptr<queue> q) noexcept;

    std::unique_ptr<queue> queue_;
};

}
