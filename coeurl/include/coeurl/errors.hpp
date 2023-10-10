#pragma once

/// @file
/// @brief Include for error to string conversions

#include <curl/curl.h>

namespace coeurl {
const char *to_string(CURLcode c);
// const char* to_string(CURLUcode c);
const char *to_string(CURLMcode c);
const char *to_string(CURLSHcode c);
} // namespace coeurl
