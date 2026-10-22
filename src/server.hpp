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
#ifdef CHATPP_ENABLE_TLS
#include <websocketpp/config/asio.hpp>
#include "tls.hpp"
#endif
#include "message.hpp"

#include <cstddef>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

namespace chatpp {

	using server = websocketpp::server<websocketpp::config::asio>;

	class chat_server_base {
	public :
		virtual ~chat_server_base() = default;

		virtual void start(const std::string& host, int port = 9002) = 0;
		virtual void stop() = 0;

		void set_server_token(std::string token) {
			server_token_ = std::move(token);
		}

		void set_max_connections(std::size_t max_connections) {
			max_connections_ = max_connections;
		}

	protected :
		std::string server_token_;
		std::size_t max_connections_ = 100;
	};

	template <typename Config>
	class basic_chat_server : public chat_server_base {
	public :
		using endpoint = websocketpp::server<Config>;
		using message_ptr = typename Config::message_type::ptr;

		basic_chat_server(websocketpp::log::level log_level = websocketpp::log::alevel::none);
		~basic_chat_server() override;
		void start(const std::string& host, int port = 9002) override;
		void stop() override;

	protected :
#ifdef CHATPP_ENABLE_TLS
		void set_tls_context(tls_context_ptr context) {
			tls_context_ = std::move(context);
		}
#endif

	private :
		struct session {
			websocketpp::connection_hdl hdl;
			std::string nickname;
		};

		void on_open(websocketpp::connection_hdl hdl);
		void on_receive_message(websocketpp::connection_hdl hdl, message_ptr msg);
		void on_close(websocketpp::connection_hdl hdl);
		void on_fail(websocketpp::connection_hdl hdl);
		void remove_session(websocketpp::connection_hdl hdl);
		void broadcast(const message& msg);
		session* get_session(websocketpp::connection_hdl hdl);

		std::vector<session> sessions_;
		endpoint server_;
		std::thread thread_;
#ifdef CHATPP_ENABLE_TLS
		tls_context_ptr tls_context_;
#endif
	};

	using chat_server = basic_chat_server<websocketpp::config::asio>;

#ifdef CHATPP_ENABLE_TLS
	class tls_chat_server : public basic_chat_server<websocketpp::config::asio_tls> {
	public :
		tls_chat_server(const std::string& cert_file, const std::string& key_file,
			websocketpp::log::level log_level = websocketpp::log::alevel::none)
			: basic_chat_server<websocketpp::config::asio_tls>(log_level) {
			set_tls_context(make_server_tls_context(cert_file, key_file));
		}
	};
#endif
}

#endif
