#include <lmdb.h>

#include "lmdb_checksum.hpp"

namespace {

MDB_crypto_funcs page_checksum_funcs{nullptr,
                                     nullptr,
                                     konsens::compute_page_checksum,
                                     0,
                                     0,
                                     static_cast<int>(konsens::page_checksum_size)};

}

extern "C" [[gnu::visibility("default")]] MDB_crypto_funcs* MDB_crypto()
{
    return &page_checksum_funcs;
}
