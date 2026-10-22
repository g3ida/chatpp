#ifndef CHATPP_AUTH_HPP
#define CHATPP_AUTH_HPP

#include <cstddef>
#include <string>

namespace chatpp {
	inline bool constant_time_equal(const std::string& a, const std::string& b) {
		if (a.size() != b.size()) {
			return false;
		}
		volatile unsigned char difference = 0;
		for (std::size_t i = 0; i < a.size(); ++i) {
			difference = static_cast<unsigned char>(
				difference | static_cast<unsigned char>(a[i] ^ b[i]));
		}
		return difference == 0;
	}
}

#endif
