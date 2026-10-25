#pragma once

#include <cstdint>

#include <lmdb.h>

namespace konsens {

inline constexpr unsigned int page_checksum_size = sizeof(std::uint32_t);

void compute_page_checksum(const MDB_val* src, MDB_val* dst, const MDB_val* key) noexcept;

}
