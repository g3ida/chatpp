#include "message.hpp"
#include <string>
#include <optional>
#include <ctime>
#include "json/json.hpp"

using namespace chatpp;
	
message::message(type t, std::string content, std::optional<unsigned int> tt) : 
	message_type(t), content(content) {
	if (tt) {
		timestamps = tt.value();
	}
	else {
		timestamps = static_cast<decltype(timestamps)>(std::time(nullptr));
	}
}