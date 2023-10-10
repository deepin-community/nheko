#pragma once

/// @file
/// @brief Include for the @ref coeurl::Headers

#include <map>
#include <string>

namespace coeurl {
/// @brief custom comparator used in the headers map
///
/// Used for caseinsensitive comparisons
struct header_less {
    //! Comparison operator, returns true if the lhs is caseinsensitively less than
    //! the rhs.
    bool operator()(const std::string &, const std::string &) const;
};

/// @brief A type to hold headers by name.
///
/// A map from a case insensitive header name to the header value.
using Headers = std::map<std::string, std::string, header_less>;
} // namespace coeurl
