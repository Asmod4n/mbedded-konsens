#include "lmdb_checksum.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

#include <crc32c/crc32c.h>

#include "byte_order.hpp"

namespace konsens {

void compute_page_checksum(const MDB_val* const src, MDB_val* const dst, const MDB_val*) noexcept
{
    const auto sum = crc32c::Crc32c(static_cast<const std::uint8_t*>(src->mv_data), src->mv_size);
    std::ranges::copy(to_little_endian(sum), static_cast<std::byte*>(dst->mv_data));
}

}
