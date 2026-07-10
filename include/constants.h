#pragma once

#include <cstddef>

using page_id_t = int;

constexpr std::size_t PAGE_SIZE = 4096;
constexpr page_id_t INVALID_PAGE_ID = -1;