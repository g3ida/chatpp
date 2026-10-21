#ifndef __DEF_MESSAGE_H___
#define __DEF_MESSAGE_H___

#ifdef _WIN32
	#include <winsock2.h>
#endif

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <string>
#include <optional>
#include <ctime>
#include "json/json.hpp"

namespace chatpp {
	
	struct message {
		enum class type {
			public_key,
			nickname,
			text
		};
		message() = default;
		message(const message&) = default;
		message(type t, std::string content, std::optional<unsigned int> tt = std::nullopt);
		type message_type = type::text;
		std::string content;
		std::string nickname;
		unsigned int timestamps = 0;
	};

	inline void to_json(nlohmann::json& j, const message& m) {
		j = nlohmann::json{ 
			{"type", static_cast<int>(m.message_type)},
			{"content", m.content},
			{"nickname", m.nickname},
			{"timestamps", m.timestamps}
		};
	}

	inline void from_json(const nlohmann::json& j, message& m) {
		j.at("type").get_to(m.message_type);
		j.at("content").get_to(m.content);
		m.nickname = j.value("nickname", std::string{});
		m.timestamps = j.value("timestamps", 0u);
	}
}
#endif