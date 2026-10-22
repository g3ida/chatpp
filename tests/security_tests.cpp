#include "auth.hpp"
#include "rate_limit.hpp"

#include <chrono>
#include <iostream>
#include <thread>

namespace {
	int failures = 0;

	void check(bool condition, const char* name) {
		if (!condition) {
			std::cerr << "FAIL: " << name << std::endl;
			++failures;
		}
	}
}

int main() {
	using chatpp::constant_time_equal;

	check(constant_time_equal("Bearer secret", "Bearer secret"), "equal tokens");
	check(!constant_time_equal("Bearer secret", "Bearer secreT"), "different tokens");
	check(!constant_time_equal("short", "longer"), "different lengths");

	chatpp::token_bucket bucket(10.0, 3.0);
	check(bucket.consume(), "burst token 1");
	check(bucket.consume(), "burst token 2");
	check(bucket.consume(), "burst token 3");
	check(!bucket.consume(), "burst exhausted");

	std::this_thread::sleep_for(std::chrono::milliseconds(250));
	check(bucket.consume(), "refilled after delay");

	if (failures == 0) {
		std::cout << "all tests passed" << std::endl;
	}
	return failures == 0 ? 0 : 1;
}
