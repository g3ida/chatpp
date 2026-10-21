#include "client.hpp"

#include <string>
#include <ctime>
#include "json/json.hpp"
#include "message.hpp"

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

using namespace std::string_literals;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

using namespace chatpp;

chat_client::chat_client(websocketpp::log::level log_level) {
	client_.set_access_channels(log_level);
	client_.clear_access_channels(websocketpp::log::alevel::frame_payload);
	// Initialize ASIO
	client_.init_asio();
	// Register our message handler
	client_.set_message_handler(bind(&chat_client::on_message, this, &client_, ::_1, ::_2));
	client_.set_open_handler(websocketpp::lib::bind(&chat_client::on_open, this, &client_, ::_1));
	client_.set_close_handler(websocketpp::lib::bind(&chat_client::on_close, this, &client_, ::_1));
	client_.set_fail_handler(websocketpp::lib::bind(&chat_client::on_fail, this, &client_, ::_1));
}

chat_client::~chat_client() {
	stop();
}

void chat_client::stop() {
	status_ = status::closed;
	msg_queue_.stop();
	client_.stop();
	if (thread_.joinable()) {
		thread_.join();
	}
	cnx_ptr_.reset();
}

void chat_client::start(std::string URI) {
	try {
		websocketpp::lib::error_code ec;
		cnx_ptr_ = client_.get_connection(URI, ec);
		if (ec) {
			status_ = status::failed;
			{
				std::lock_guard<std::mutex> lk(status_mutex_);
				status_msg_ = ec.message();
			}
			throw websocketpp::exception(ec);
		}
		// Note that connect here only requests a connection. No network messages are
		// exchanged until the event loop starts running in the next line.
		cnx_ptr_ = client_.connect(cnx_ptr_);
		status_ = status::connecting;
		thread_ = std::thread([this]() { client_.run(); });
	}
	catch (websocketpp::exception const& e) {
		status_ = status::failed;
		{
			std::lock_guard<std::mutex> lk(status_mutex_);
			status_msg_ = e.m_code.message();
		}
		throw;
	}
}

void chat_client::send(std::string msg) {
	nlohmann::json msg_json_object;
	to_json(msg_json_object, message(message::type::text, msg));
	std::string json_str = msg_json_object.dump();
	client_.send(cnx_ptr_->get_handle(), json_str.data(), json_str.size(), websocketpp::frame::opcode::text);
}

void chat_client::send_nickname(std::string nickname) {
	nlohmann::json msg_json_object;
	to_json(msg_json_object, message(message::type::nickname, nickname));
	std::string json_str = msg_json_object.dump();
	client_.send(cnx_ptr_->get_handle(), json_str.data(), json_str.size(), websocketpp::frame::opcode::text);
}

chat_client::status chat_client::getStatus() const {
	return status_.load();
}

std::string chat_client::getServerStr() {
	std::lock_guard<std::mutex> lk(status_mutex_);
	return server_str_;
}

std::string chat_client::getErrorMessage() {
	std::lock_guard<std::mutex> lk(status_mutex_);
	return status_msg_;
}

void chat_client::on_open(client_t* client, websocketpp::connection_hdl hdl) {
	status_ = status::open;
	client_t::connection_ptr con = client->get_con_from_hdl(hdl);
	std::lock_guard<std::mutex> lk(status_mutex_);
	server_str_ = con->get_response_header("Server");
}

void chat_client::on_close(client_t* client, websocketpp::connection_hdl hdl) {
	status_ = status::closed;
	client_t::connection_ptr con = client->get_con_from_hdl(hdl);
	std::stringstream s;
	s << "close code: " << con->get_remote_close_code() << " ("
		<< websocketpp::close::status::get_string(con->get_remote_close_code())
		<< "), close reason: " << con->get_remote_close_reason();
	std::lock_guard<std::mutex> lk(status_mutex_);
	server_str_ = s.str();
}

void chat_client::on_fail(client_t* client, websocketpp::connection_hdl hdl) {
	status_ = status::failed;
	client_t::connection_ptr con = client->get_con_from_hdl(hdl);
	std::lock_guard<std::mutex> lk(status_mutex_);
	server_str_ = con->get_response_header("Server");
	status_msg_ = con->get_ec().message();
}

void chat_client::on_message(websocketpp::client<websocketpp::config::asio_client>*, websocketpp::connection_hdl, client_message_ptr_t msg) {
	if (msg->get_opcode() != websocketpp::frame::opcode::text) {
		return;
	}
	try {
		message m = nlohmann::json::parse(msg->get_payload()).get<message>();
		if (m.message_type == message::type::text) {
			msg_queue_.enqueue(std::make_pair(std::move(m.nickname), std::move(m.content)));
		}
	}
	catch (std::exception& e) {
		std::lock_guard<std::mutex> lk(status_mutex_);
		status_msg_ = e.what();
	}
}