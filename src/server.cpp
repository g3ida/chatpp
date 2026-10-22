#include "server.hpp"

#include <websocketpp/server.hpp>

#include "message.hpp"

#include <stdexcept>
#include <string>
#include <vector>

using namespace chatpp;

template <typename Config>
basic_chat_server<Config>::basic_chat_server(websocketpp::log::level log_level) {
	// Set logging settings
	server_.set_access_channels(log_level);
	server_.clear_access_channels(websocketpp::log::alevel::frame_payload);
	server_.set_max_message_size(64 * 1024);
	// Initialize Asio
	server_.init_asio();
	// Register our handlers
	server_.set_message_handler(bind(&basic_chat_server::on_receive_message, this, ::_1, ::_2));
	server_.set_open_handler(bind(&basic_chat_server::on_open, this, ::_1));
	server_.set_close_handler(bind(&basic_chat_server::on_close, this, ::_1));
	server_.set_fail_handler(bind(&basic_chat_server::on_fail, this, ::_1));
#ifdef CHATPP_ENABLE_TLS
	if constexpr (std::is_same<Config, websocketpp::config::asio_tls>::value) {
		server_.set_tls_init_handler([this](websocketpp::connection_hdl) {
			return tls_context_;
		});
	}
#endif
}

template <typename Config>
basic_chat_server<Config>::~basic_chat_server() {
	stop();
}

template <typename Config>
void basic_chat_server<Config>::stop() {
	server_.stop();
	if (thread_.joinable()) {
		thread_.join();
	}
	if (server_.is_listening()) {
		websocketpp::lib::error_code ec;
		server_.stop_listening(ec);
	}
}

template <typename Config>
void basic_chat_server<Config>::start(const std::string& host, int port) {
#ifdef CHATPP_ENABLE_TLS
	if constexpr (std::is_same<Config, websocketpp::config::asio_tls>::value) {
		if (!tls_context_) {
			throw std::runtime_error("TLS is not configured");
		}
	}
#endif
	server_.listen(host, std::to_string(port));
	// Start the server accept loop
	server_.start_accept();
	// Start the ASIO io_service run loop in another thread
	thread_ = std::thread([this]() { server_.run(); });
}

template <typename Config>
void basic_chat_server<Config>::on_open(websocketpp::connection_hdl hdl) {
	sessions_.push_back({ hdl, {} });
}

// Callback to handle incoming messages
template <typename Config>
void basic_chat_server<Config>::on_receive_message(websocketpp::connection_hdl hdl, message_ptr msg) {
	if (msg->get_opcode() != websocketpp::frame::opcode::text) {
		return;
	}
	try {
		message m = nlohmann::json::parse(msg->get_payload()).template get<message>();
		switch (m.message_type) {
		case message::type::nickname:
			if (m.content.empty() || m.content.size() > 32) {
				break;
			}
			if (auto* s = get_session(hdl)) {
				s->nickname = m.content;
			}
			break;
		case message::type::public_key:
			break;
		case message::type::text:
			if (auto* s = get_session(hdl); s && !s->nickname.empty()) {
				m.nickname = s->nickname;
				broadcast(m);
			}
			break;
		}
	}
	catch (std::exception&) {
	}
}

template <typename Config>
typename basic_chat_server<Config>::session* basic_chat_server<Config>::get_session(websocketpp::connection_hdl hdl) {
	auto target = hdl.lock();
	if (!target) {
		return nullptr;
	}
	for (auto& s : sessions_) {
		auto current = s.hdl.lock();
		if (current && current == target) {
			return &s;
		}
	}
	return nullptr;
}

template <typename Config>
void basic_chat_server<Config>::remove_session(websocketpp::connection_hdl hdl) {
	auto target = hdl.lock();
	if (!target) {
		return;
	}
	for (auto it = sessions_.begin(); it != sessions_.end(); ++it) {
		auto current = it->hdl.lock();
		if (current && current == target) {
			sessions_.erase(it);
			return;
		}
	}
}

template <typename Config>
void basic_chat_server<Config>::broadcast(const message& msg) {
	nlohmann::json j;
	to_json(j, msg);
	const std::string payload = j.dump();

	for (auto it = sessions_.begin(); it != sessions_.end();) {
		if (it->hdl.expired()) {
			it = sessions_.erase(it);
			continue;
		}

		websocketpp::lib::error_code ec;
		server_.send(it->hdl, payload, websocketpp::frame::opcode::text, ec);
		if (ec) {
			it = sessions_.erase(it);
		}
		else {
			++it;
		}
	}
}

template <typename Config>
void basic_chat_server<Config>::on_close(websocketpp::connection_hdl hdl) {
	remove_session(hdl);
}

template <typename Config>
void basic_chat_server<Config>::on_fail(websocketpp::connection_hdl hdl) {
	remove_session(hdl);
}

template class chatpp::basic_chat_server<websocketpp::config::asio>;
#ifdef CHATPP_ENABLE_TLS
template class chatpp::basic_chat_server<websocketpp::config::asio_tls>;
#endif
