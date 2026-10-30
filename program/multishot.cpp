#include <cerrno>
#include <stdexcept>

#include <liburing.h>

#include "multishot.hpp"

namespace konsensd {

receive_outcome receive_outcome_of(const int res, const std::uint32_t flags)
{
    const bool more = (flags & IORING_CQE_F_MORE) != 0;
    const bool buffer = (flags & IORING_CQE_F_BUFFER) != 0;
    if (res > 0) {
        if (!buffer) [[unlikely]] {
            throw std::logic_error("konsensd: a multishot recv gave bytes without a buffer");
        }
        return more ? receive_outcome::bytes_and_armed : receive_outcome::bytes_and_ended;
    }
    if (more) [[unlikely]] {
        throw std::logic_error("konsensd: a multishot recv stays armed after it gave no bytes");
    }
    if (res == -EINVAL) [[unlikely]] {
        throw std::logic_error("konsensd: io_uring refuses the multishot recv with a provided buffer ring (EINVAL)");
    }
    if (res == 0) {
        return receive_outcome::end_of_stream;
    }
    if (res == -ENOBUFS) [[unlikely]] {
        return receive_outcome::no_buffer;
    }
    return receive_outcome::error;
}

accept_outcome accept_outcome_of(const int res, const std::uint32_t flags)
{
    const bool more = (flags & IORING_CQE_F_MORE) != 0;
    if (res >= 0) {
        return more ? accept_outcome::descriptor_and_armed : accept_outcome::descriptor_and_ended;
    }
    if (more) [[unlikely]] {
        throw std::logic_error("konsensd: a multishot accept stays armed after an error");
    }
    if (res == -EINVAL) [[unlikely]] {
        throw std::logic_error("konsensd: io_uring refuses the multishot accept (EINVAL)");
    }
    return accept_outcome::error;
}

}
