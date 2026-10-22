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
#include "auth.hpp"
#include "message.hpp"
#include "rate_limit.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <thread>
#include <type_traits>
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

		void set_max_connections_per_ip(std::size_t max_connections_per_ip) {
			max_connections_per_ip_ = max_connections_per_ip;
		}

	protected :
		std::string server_token_;
		std::size_t max_connections_ = 100;
		std::size_t max_connections_per_ip_ = 5;
		std::size_t max_buffered_amount_ = 1024 * 1024;
		double message_rate_ = 20.0;
		double message_burst_ = 40.0;
		double byte_rate_ = 256.0 * 1024.0;
		double byte_burst_ = 512.0 * 1024.0;
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
			std::string remote_ip;
			token_bucket messages;
			token_bucket bytes;
		};

		bool on_validate(websocketpp::connection_hdl hdl);
		void on_open(websocketpp::connection_hdl hdl);
		void on_receive_message(websocketpp::connection_hdl hdl, message_ptr msg);
		void on_close(websocketpp::connection_hdl hdl);
		void on_fail(websocketpp::connection_hdl hdl);
		void remove_session(websocketpp::connection_hdl hdl);
		void release_ip(const std::string& ip);
		void close_session(websocketpp::connection_hdl hdl, const std::string& reason);
		void broadcast(const message& msg);
		session* get_session(websocketpp::connection_hdl hdl);

		std::vector<session> sessions_;
		std::map<std::string, std::size_t> ip_counts_;
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
