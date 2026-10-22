#include "server.hpp"

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "message.hpp"

#include <string>
#include <vector>

using namespace chatpp;

chat_server::chat_server(websocketpp::log::level log_level) {
	// Set logging settings
	server_.set_access_channels(log_level);
	server_.clear_access_channels(websocketpp::log::alevel::frame_payload);
	server_.set_max_message_size(64 * 1024);
	// Initialize Asio
	server_.init_asio();			
	// Register our message handler
	server_.set_message_handler(bind(&chat_server::on_receive_message, this, &server_, ::_1, ::_2));
	server_.set_close_handler(bind(&chat_server::on_close, this, &server_, ::_1));
	server_.set_fail_handler(bind(&chat_server::on_fail, this, &server_, ::_1));
}

chat_server::~chat_server() {
	stop();
}

void chat_server::stop() {
	server_.stop();
	if (thread_.joinable()) {
		thread_.join();
	}
	if (server_.is_listening()) {
		websocketpp::lib::error_code ec;
		server_.stop_listening(ec);
	}
}

// Callback to handle incoming messages
void chat_server::on_receive_message(server*, websocketpp::connection_hdl hdl, client_message_ptr_t msg) {
	if (msg->get_opcode() != websocketpp::frame::opcode::text) {
		return;
	}
	try {
		message m = nlohmann::json::parse(msg->get_payload()).get<message>();
		switch (m.message_type) {
		case message::type::nickname:
			if (m.content.empty() || m.content.size() > 32) {
				break;
			}
			if (auto* s = get_session(hdl)) {
				s->nickname = m.content;
			}
			else {
				sessions_.push_back({ hdl, m.content });
			}
			break;
		case message::type::public_key:
			break;
		case message::type::text:
			if (auto* s = get_session(hdl)) {
				m.nickname = s->nickname;
				broadcast(m);
			}
			break;
		}
	}
	catch (std::exception&) {
	}
}

void chat_server::start(const std::string& host, int port) {
	server_.listen(host, std::to_string(port));
	// Start the server accept loop
	server_.start_accept();
	// Start the ASIO io_service run loop in another thread
	thread_ = std::thread([this]() { server_.run(); });
}

chat_server::session* chat_server::get_session(websocketpp::connection_hdl hdl) {
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

void chat_server::remove_session(websocketpp::connection_hdl hdl) {
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

void chat_server::broadcast(const message& msg) {
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

void chat_server::on_close(server*, websocketpp::connection_hdl hdl) {
	remove_session(hdl);
}

void chat_server::on_fail(server*, websocketpp::connection_hdl hdl) {
	remove_session(hdl);
}
