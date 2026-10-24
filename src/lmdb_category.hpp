#pragma once

#include <system_error>

namespace konsens {

const std::error_category& lmdb_category() noexcept;

}
