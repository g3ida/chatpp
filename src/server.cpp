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
			if (auto it = get_nickname(hdl); it != nicknames_.end()) {
				(*it) = m.content;
			}
			else {
				handlers_.emplace_back(hdl);
				nicknames_.emplace_back(m.content);
			}
			break;
		case message::type::public_key:
			break;
		case message::type::text:
			if (auto it = get_nickname(hdl); it != nicknames_.end()) {
				m.nickname = *it;
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

std::vector<std::string>::iterator chat_server::get_nickname(websocketpp::connection_hdl hdl) {
	if (!hdl.expired()) {
		auto cnx = hdl.lock();
		for (size_t i = 0; i < handlers_.size(); ++i) {
			if (!handlers_[i].expired()) {
				if (handlers_[i].lock() == cnx) {
					return (nicknames_.begin() + i);
				}
			}
		}
	}
	return nicknames_.end();
}

void chat_server::remove_handler(websocketpp::connection_hdl hdl) {
	auto target = hdl.lock();
	if (!target) {
		return;
	}
	for (size_t i = 0; i < handlers_.size(); ++i) {
		auto current = handlers_[i].lock();
		if (current && current == target) {
			handlers_.erase(handlers_.begin() + i);
			nicknames_.erase(nicknames_.begin() + i);
			return;
		}
	}
}

void chat_server::broadcast(const message& msg) {
	nlohmann::json j;
	to_json(j, msg);
	const std::string payload = j.dump();

	for (size_t i = 0; i < handlers_.size();) {
		auto hdl = handlers_[i].lock();
		if (!hdl) {
			handlers_.erase(handlers_.begin() + i);
			nicknames_.erase(nicknames_.begin() + i);
			continue;
		}

		websocketpp::lib::error_code ec;
		server_.send(handlers_[i], payload, websocketpp::frame::opcode::text, ec);
		if (ec) {
			handlers_.erase(handlers_.begin() + i);
			nicknames_.erase(nicknames_.begin() + i);
		}
		else {
			++i;
		}
	}
}

void chat_server::on_close(server*, websocketpp::connection_hdl hdl) {
	remove_handler(hdl);
}

void chat_server::on_fail(server*, websocketpp::connection_hdl hdl) {
	remove_handler(hdl);
}
