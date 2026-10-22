#include "server.hpp"

#include <websocketpp/server.hpp>

#include "message.hpp"

#include <stdexcept>
#include <string>
#include <vector>

using namespace chatpp;

namespace {
	std::string remote_ip(const std::string& endpoint) {
		if (endpoint.empty()) {
			return {};
		}
		if (endpoint.front() == '[') {
			const auto closing = endpoint.find(']');
			if (closing == std::string::npos) {
				return {};
			}
			return endpoint.substr(1, closing - 1);
		}
		if (endpoint.find(':') != endpoint.rfind(':')) {
			return endpoint;
		}
		const auto colon = endpoint.rfind(':');
		if (colon == std::string::npos) {
			return endpoint;
		}
		return endpoint.substr(0, colon);
	}
}

template <typename Config>
basic_chat_server<Config>::basic_chat_server(websocketpp::log::level log_level) {
	// Set logging settings
	server_.set_access_channels(log_level);
	server_.clear_access_channels(websocketpp::log::alevel::frame_payload);
	server_.set_max_message_size(64 * 1024);
	server_.set_reuse_addr(true);
	// Initialize Asio
	server_.init_asio();
	// Register our handlers
	server_.set_validate_handler(bind(&basic_chat_server::on_validate, this, ::_1));
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
bool basic_chat_server<Config>::on_validate(websocketpp::connection_hdl hdl) {
	websocketpp::lib::error_code ec;
	auto con = server_.get_con_from_hdl(hdl, ec);
	if (ec) {
		return false;
	}
	if (max_connections_ > 0 && sessions_.size() >= max_connections_) {
		con->set_status(websocketpp::http::status_code::service_unavailable, "Too many connections");
		return false;
	}
	if (max_connections_per_ip_ > 0) {
		const std::string ip = remote_ip(con->get_remote_endpoint());
		if (!ip.empty()) {
			auto found = ip_counts_.find(ip);
			if (found != ip_counts_.end() && found->second >= max_connections_per_ip_) {
				con->set_status(websocketpp::http::status_code::too_many_requests,
					"Too many connections from this address");
				return false;
			}
		}
	}
	if (!server_token_.empty()) {
		const std::string expected = "Bearer " + server_token_;
		if (!constant_time_equal(con->get_request_header("Authorization"), expected)) {
			con->set_status(websocketpp::http::status_code::unauthorized, "Unauthorized");
			return false;
		}
	}
	return true;
}

template <typename Config>
void basic_chat_server<Config>::on_open(websocketpp::connection_hdl hdl) {
	std::string ip;
	websocketpp::lib::error_code ec;
	auto con = server_.get_con_from_hdl(hdl, ec);
	if (!ec) {
		ip = remote_ip(con->get_remote_endpoint());
	}
	if (!ip.empty()) {
		++ip_counts_[ip];
	}
	sessions_.push_back({
		hdl,
		{},
		std::move(ip),
		token_bucket(message_rate_, message_burst_),
		token_bucket(byte_rate_, byte_burst_)
	});
}

// Callback to handle incoming messages
template <typename Config>
void basic_chat_server<Config>::on_receive_message(websocketpp::connection_hdl hdl, message_ptr msg) {
	if (msg->get_opcode() != websocketpp::frame::opcode::text) {
		return;
	}
	auto* s = get_session(hdl);
	if (!s) {
		return;
	}
	const double payload_size = static_cast<double>(msg->get_payload().size());
	if (!s->messages.consume() || !s->bytes.consume(payload_size)) {
		close_session(hdl, "rate limit exceeded");
		return;
	}
	try {
		message m = nlohmann::json::parse(msg->get_payload()).template get<message>();
		switch (m.message_type) {
		case message::type::nickname:
			if (!m.content.empty() && m.content.size() <= 32) {
				s->nickname = m.content;
			}
			break;
		case message::type::public_key:
			break;
		case message::type::text:
			if (!s->nickname.empty()) {
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
			const std::string ip = it->remote_ip;
			sessions_.erase(it);
			release_ip(ip);
			return;
		}
	}
}

template <typename Config>
void basic_chat_server<Config>::release_ip(const std::string& ip) {
	if (ip.empty()) {
		return;
	}
	auto it = ip_counts_.find(ip);
	if (it == ip_counts_.end()) {
		return;
	}
	if (--it->second == 0) {
		ip_counts_.erase(it);
	}
}

template <typename Config>
void basic_chat_server<Config>::close_session(websocketpp::connection_hdl hdl, const std::string& reason) {
	websocketpp::lib::error_code ec;
	server_.close(hdl, websocketpp::close::status::policy_violation, reason, ec);
	remove_session(hdl);
}

template <typename Config>
void basic_chat_server<Config>::broadcast(const message& msg) {
	nlohmann::json j;
	to_json(j, msg);
	const std::string payload = j.dump();

	for (auto it = sessions_.begin(); it != sessions_.end();) {
		websocketpp::lib::error_code ec;
		auto con = server_.get_con_from_hdl(it->hdl, ec);
		if (ec) {
			const std::string ip = it->remote_ip;
			it = sessions_.erase(it);
			release_ip(ip);
			continue;
		}
		if (con->get_buffered_amount() > max_buffered_amount_) {
			server_.close(it->hdl, websocketpp::close::status::policy_violation,
				"too much buffered data", ec);
			const std::string ip = it->remote_ip;
			it = sessions_.erase(it);
			release_ip(ip);
			continue;
		}
		server_.send(it->hdl, payload, websocketpp::frame::opcode::text, ec);
		if (ec) {
			const std::string ip = it->remote_ip;
			it = sessions_.erase(it);
			release_ip(ip);
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
