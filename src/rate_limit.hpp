#ifndef __DEF_RATE_LIMIT_H___
#define __DEF_RATE_LIMIT_H___

#include <algorithm>
#include <chrono>

namespace chatpp {
	class token_bucket {
	public :
		using clock = std::chrono::steady_clock;

		token_bucket(double rate, double burst)
			: rate_(rate), burst_(burst), tokens_(burst), last_(clock::now()) {
		}

		bool consume(double amount = 1.0) {
			const auto now = clock::now();
			const double elapsed = std::chrono::duration<double>(now - last_).count();
			last_ = now;
			tokens_ = std::min(burst_, tokens_ + elapsed * rate_);
			if (tokens_ < amount) {
				return false;
			}
			tokens_ -= amount;
			return true;
		}

	private :
		double rate_;
		double burst_;
		double tokens_;
		clock::time_point last_;
	};
}

#endif
