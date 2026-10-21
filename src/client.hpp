#ifndef __DEF_CLIENT_H___
#define __DEF_CLIENT_H___

#ifdef _WIN32
	#include <winsock2.h>
#endif

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_TYPE_TRAITS_
#define _WEBSOCKETPP_CPP11_RANDOM_DEVICE_
#define _WEBSOCKETPP_CPP11_THREAD_
#endif

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include "queue.hpp"

#include <atomic>
#include <mutex>
#include <string>

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

namespace chatpp {

	using client_t = websocketpp::client<websocketpp::config::asio_client>;
	using client_connection_ptr_t = websocketpp::client<websocketpp::config::asio_client>::connection_ptr;
	using client_message_ptr_t = websocketpp::config::asio_client::message_type::ptr;

	class chat_client {
	public :
		enum class status {
			open,
			closed,
			failed,
			connecting
		};
		chat_client(websocketpp::log::level log_level = websocketpp::log::alevel::none);
		void start(std::string URI);
		void send(std::string msg);
		void send_nickname(std::string nickname);
		status getStatus() const;
		std::string getServerStr();
		std::string getErrorMessage();

		message_queue msg_queue_;

	private :
		void on_open(client_t* client, websocketpp::connection_hdl hdl);
		void on_close(client_t* client, websocketpp::connection_hdl hdl);
		void on_fail(client_t* client, websocketpp::connection_hdl hdl);
		void on_message(websocketpp::client<websocketpp::config::asio_client>* c, websocketpp::connection_hdl hdl, client_message_ptr_t msg);
		std::atomic<status> status_{ status::closed };
		client_connection_ptr_t cnx_ptr_;
		websocketpp::connection_hdl hdl_;
		client_t client_;
		std::string server_str_;
		std::string status_msg_;
		mutable std::mutex status_mutex_;
	};
}
#endif