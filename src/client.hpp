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
#ifdef CHATPP_ENABLE_TLS
#include <websocketpp/config/asio_client.hpp>
#include "tls.hpp"
#endif
#include "message.hpp"
#include "queue.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

namespace chatpp {

	class chat_client_base {
	public :
		enum class status {
			open,
			closed,
			failed,
			connecting
		};
		virtual ~chat_client_base() = default;

		virtual void start(const std::string& URI) = 0;
		virtual void stop() = 0;
		virtual void send(std::string msg) = 0;
		virtual void send_nickname(std::string nickname) = 0;
		virtual status getStatus() const = 0;
		virtual std::string getServerStr() = 0;
		virtual std::string getErrorMessage() = 0;

		void set_auth_token(std::string token) {
			token_ = std::move(token);
		}

#ifdef CHATPP_ENABLE_TLS
		void set_tls_options(std::string ca_file, bool insecure) {
			ca_file_ = std::move(ca_file);
			insecure_ = insecure;
		}
#endif

		message_queue msg_queue_;

	protected :
		std::string token_;
#ifdef CHATPP_ENABLE_TLS
		std::string ca_file_;
		bool insecure_ = false;
#endif
	};

	template <typename Config>
	class basic_chat_client : public chat_client_base {
	public :
		using endpoint = websocketpp::client<Config>;
		using connection_ptr = typename endpoint::connection_ptr;
		using message_ptr = typename Config::message_type::ptr;

		basic_chat_client(websocketpp::log::level log_level = websocketpp::log::alevel::none);
		~basic_chat_client() override;
		void start(const std::string& URI) override;
		void stop() override;
		void send(std::string msg) override;
		void send_nickname(std::string nickname) override;
		status getStatus() const override;
		std::string getServerStr() override;
		std::string getErrorMessage() override;

	private :
		void on_open(websocketpp::connection_hdl hdl);
		void on_close(websocketpp::connection_hdl hdl);
		void on_fail(websocketpp::connection_hdl hdl);
		void on_message(websocketpp::connection_hdl hdl, message_ptr msg);
		void send_message(const message& msg);

		std::atomic<status> status_{ status::closed };
		endpoint client_;
		connection_ptr cnx_ptr_;
		std::string server_str_;
		std::string status_msg_;
		mutable std::mutex status_mutex_;
		std::thread thread_;
#ifdef CHATPP_ENABLE_TLS
		tls_context_ptr tls_context_;
#endif
	};

	using chat_client = basic_chat_client<websocketpp::config::asio_client>;

#ifdef CHATPP_ENABLE_TLS
	using tls_chat_client = basic_chat_client<websocketpp::config::asio_tls_client>;
#endif
}
#endif
