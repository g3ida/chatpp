#include "client.hpp"

#include <sstream>
#include <string>
#include <ctime>
#include <utility>
#include "json/json.hpp"

#include <websocketpp/client.hpp>

using namespace std::string_literals;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

using namespace chatpp;

template <typename Config>
basic_chat_client<Config>::basic_chat_client(websocketpp::log::level log_level) {
	client_.set_access_channels(log_level);
	client_.clear_access_channels(websocketpp::log::alevel::frame_payload);
	client_.set_max_message_size(64 * 1024);
	// Initialize ASIO
	client_.init_asio();
	// Register our handlers
	client_.set_message_handler(bind(&basic_chat_client::on_message, this, ::_1, ::_2));
	client_.set_open_handler(bind(&basic_chat_client::on_open, this, ::_1));
	client_.set_close_handler(bind(&basic_chat_client::on_close, this, ::_1));
	client_.set_fail_handler(bind(&basic_chat_client::on_fail, this, ::_1));
#ifdef CHATPP_ENABLE_TLS
	if constexpr (std::is_same<Config, websocketpp::config::asio_tls_client>::value) {
		client_.set_tls_init_handler([this](websocketpp::connection_hdl) {
			return tls_context_;
		});
	}
#endif
}

template <typename Config>
basic_chat_client<Config>::~basic_chat_client() {
	stop();
}

template <typename Config>
void basic_chat_client<Config>::stop() {
	status_ = status::closed;
	msg_queue_.stop();
	client_.stop();
	if (thread_.joinable()) {
		thread_.join();
	}
	cnx_ptr_.reset();
}

template <typename Config>
void basic_chat_client<Config>::start(const std::string& URI) {
	try {
#ifdef CHATPP_ENABLE_TLS
		if constexpr (std::is_same<Config, websocketpp::config::asio_tls_client>::value) {
			websocketpp::uri parsed_uri(URI);
			tls_context_ = make_client_tls_context(parsed_uri.get_host(), ca_file_, insecure_);
		}
#endif
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
		if (!token_.empty()) {
			cnx_ptr_->append_header("Authorization", "Bearer " + token_);
		}
		// Note that connect here only requests a connection. No network messages are
		// exchanged until the event loop starts running in the next line.
		cnx_ptr_ = client_.connect(cnx_ptr_);
		status_ = status::connecting;
		thread_ = std::thread([this]() { client_.run(); });
	}
	catch (std::exception const& e) {
		status_ = status::failed;
		{
			std::lock_guard<std::mutex> lk(status_mutex_);
			status_msg_ = e.what();
		}
		throw;
	}
}

template <typename Config>
void basic_chat_client<Config>::send(std::string msg) {
	send_message(message(message::type::text, std::move(msg)));
}

template <typename Config>
void basic_chat_client<Config>::send_nickname(std::string nickname) {
	send_message(message(message::type::nickname, std::move(nickname)));
}

template <typename Config>
void basic_chat_client<Config>::send_message(const message& msg) {
	if (!cnx_ptr_ || getStatus() != status::open) {
		return;
	}
	nlohmann::json msg_json_object;
	to_json(msg_json_object, msg);
	const std::string json_str = msg_json_object.dump();
	websocketpp::lib::error_code ec;
	client_.send(cnx_ptr_->get_handle(), json_str, websocketpp::frame::opcode::text, ec);
	if (ec) {
		std::lock_guard<std::mutex> lk(status_mutex_);
		status_msg_ = ec.message();
	}
}

template <typename Config>
typename basic_chat_client<Config>::status basic_chat_client<Config>::getStatus() const {
	return status_.load();
}

template <typename Config>
std::string basic_chat_client<Config>::getServerStr() {
	std::lock_guard<std::mutex> lk(status_mutex_);
	return server_str_;
}

template <typename Config>
std::string basic_chat_client<Config>::getErrorMessage() {
	std::lock_guard<std::mutex> lk(status_mutex_);
	return status_msg_;
}

template <typename Config>
void basic_chat_client<Config>::on_open(websocketpp::connection_hdl hdl) {
	status_ = status::open;
	auto con = client_.get_con_from_hdl(hdl);
	std::lock_guard<std::mutex> lk(status_mutex_);
	server_str_ = con->get_response_header("Server");
}

template <typename Config>
void basic_chat_client<Config>::on_close(websocketpp::connection_hdl hdl) {
	status_ = status::closed;
	auto con = client_.get_con_from_hdl(hdl);
	std::stringstream s;
	s << "close code: " << con->get_remote_close_code() << " ("
		<< websocketpp::close::status::get_string(con->get_remote_close_code())
		<< "), close reason: " << con->get_remote_close_reason();
	std::lock_guard<std::mutex> lk(status_mutex_);
	server_str_ = s.str();
}

template <typename Config>
void basic_chat_client<Config>::on_fail(websocketpp::connection_hdl hdl) {
	status_ = status::failed;
	auto con = client_.get_con_from_hdl(hdl);
	std::lock_guard<std::mutex> lk(status_mutex_);
	server_str_ = con->get_response_header("Server");
	status_msg_ = con->get_ec().message();
}

template <typename Config>
void basic_chat_client<Config>::on_message(websocketpp::connection_hdl, message_ptr msg) {
	if (msg->get_opcode() != websocketpp::frame::opcode::text) {
		return;
	}
	try {
		message m = nlohmann::json::parse(msg->get_payload()).template get<message>();
		if (m.message_type == message::type::text) {
			msg_queue_.enqueue(std::make_pair(std::move(m.nickname), std::move(m.content)));
		}
	}
	catch (std::exception& e) {
		std::lock_guard<std::mutex> lk(status_mutex_);
		status_msg_ = e.what();
	}
}

template class chatpp::basic_chat_client<websocketpp::config::asio_client>;
#ifdef CHATPP_ENABLE_TLS
template class chatpp::basic_chat_client<websocketpp::config::asio_tls_client>;
#endif
