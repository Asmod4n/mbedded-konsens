#include "konsens/ring.hpp"

#include <cerrno>
#include <stdexcept>
#include <utility>

#include <liburing.h>

namespace konsens {

struct ring::queue {
    io_uring uring;

    explicit queue(const io_uring& initialized) noexcept : uring{initialized} {}
    queue(const queue&) = delete;
    queue& operator=(const queue&) = delete;
    ~queue() { io_uring_queue_exit(&uring); }
};

namespace {

std::error_code error_of(const int negative_errno) noexcept
{
    return {-negative_errno, std::system_category()};
}

}

std::expected<ring, std::error_code> ring::with(const unsigned entries)
{
    io_uring u{};
    if (const int r = io_uring_queue_init(entries, &u, 0); r < 0) [[unlikely]] {
        return std::unexpected(error_of(r));
    }
    return ring{std::make_unique<queue>(u)};
}

std::expected<void, std::error_code> ring::msg_ring(const ring& to, const std::uint64_t user_data)
{
    if (!queue_ || !to.queue_) [[unlikely]] {
        throw std::logic_error("konsens: a ring was moved from");
    }
    io_uring_sqe* const s = io_uring_get_sqe(&queue_->uring);
    if (s == nullptr) [[unlikely]] {
        return std::unexpected(std::make_error_code(std::errc::no_buffer_space));
    }
    io_uring_prep_msg_ring(s, to.queue_->uring.ring_fd, 0, user_data, 0);
    io_uring_sqe_set_flags(s, IOSQE_CQE_SKIP_SUCCESS);
    if (const int r = io_uring_submit(&queue_->uring); r < 0) [[unlikely]] {
        return std::unexpected(error_of(r));
    }
    return {};
}

std::expected<std::uint64_t, std::error_code> ring::wait_cqe()
{
    if (!queue_) [[unlikely]] {
        throw std::logic_error("konsens: a ring was moved from");
    }
    io_uring_cqe* c = nullptr;
    int r = io_uring_wait_cqe(&queue_->uring, &c);
    while (r == -EINTR) [[unlikely]] {
        r = io_uring_wait_cqe(&queue_->uring, &c);
    }
    if (r < 0) [[unlikely]] {
        return std::unexpected(error_of(r));
    }
    const int res = c->res;
    const std::uint64_t user_data = c->user_data;
    io_uring_cqe_seen(&queue_->uring, c);
    if (res < 0) [[unlikely]] {
        return std::unexpected(error_of(res));
    }
    return user_data;
}

ring::ring(std::unique_ptr<queue> q) noexcept : queue_{std::move(q)}
{
}

ring::ring(ring&&) noexcept = default;
ring& ring::operator=(ring&&) noexcept = default;
ring::~ring() = default;

}
