#include "message.hpp"
#include <string>
#include <optional>
#include <ctime>
#include "json/json.hpp"

using namespace chatpp;
	
message::message(type t, std::string content, std::optional<std::time_t> tt) :
	message_type(t), content(content) {
	if (tt) {
		timestamps = *tt;
	}
	else {
		timestamps = std::time(nullptr);
	}
}