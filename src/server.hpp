#ifndef __DEF_SERVER_H___
#define __DEF_SERVER_H___

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

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include "client.hpp"
#include "message.hpp"

#include <string>
#include <thread>
#include <vector>

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

namespace chatpp {

	using server = websocketpp::server<websocketpp::config::asio>;

	class chat_server {
	public :
		chat_server(websocketpp::log::level log_level = websocketpp::log::alevel::none);
		~chat_server();
		void start(const std::string& host, int port = 9002);
		void stop();
	private :
		struct session {
			websocketpp::connection_hdl hdl;
			std::string nickname;
		};

		void on_receive_message(server*, websocketpp::connection_hdl hdl, client_message_ptr_t msg);
		void on_close(server*, websocketpp::connection_hdl hdl);
		void on_fail(server*, websocketpp::connection_hdl hdl);
		void remove_session(websocketpp::connection_hdl hdl);
		void broadcast(const message& msg);
		session* get_session(websocketpp::connection_hdl hdl);

		std::vector<session> sessions_;
		websocketpp::server<websocketpp::config::asio> server_;
		std::thread thread_;
	};
}

#endif
