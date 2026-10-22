#include "rang.hpp"
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include "queue.hpp"
#include "client.hpp"
#include "server.hpp"
#include "CLI11.hpp"

using namespace std::chrono_literals;

namespace {
	std::string sanitize(std::string text) {
		for (auto& c : text) {
			auto value = static_cast<unsigned char>(c);
			if (value < 0x20 || value == 0x7f) {
				c = '?';
			}
		}
		return text;
	}

	bool is_loopback(const std::string& host) {
		return host == "127.0.0.1" || host == "::1" || host == "localhost";
	}
}

int main(int argc, char* argv[]) {
	// Create a server endpoint

	try {

		const std::string chatpp_ascii_art_logo = 
			R"(
  ___  _   _    __   ____   _      _   
 / __)( )_( )  /__\ (_  _)_| |_  _| |_ 
( (__  ) _ (  /(__)\  )( (_   _)(_   _)
 \___)(_) (_)(__)(__)(__)  |_|    |_|  

			)";

		std::cout << chatpp_ascii_art_logo << std::endl;
		//std::cout << create_model() << std::endl;
		CLI::App app{ "Chat++" };

		int port = 9002;
		std::string host = "127.0.0.1";
		std::string URI;
		std::string tls_cert;
		std::string tls_key;
		std::string tls_ca;
		std::string token;
		std::string server_token;
		bool insecure = false;
		bool allow_plaintext = false;
		int max_connections = 100;

		//add configuration file
		app.set_config("--conf")->default_val("conf.ini");

		app.add_option("-H,--host", host,
			"specify the listening address")->default_val("127.0.0.1");

		app.add_option("-p,--port", port,
			"specify the listening port")->default_val("9002");

		app.add_option("--tls-cert", tls_cert,
			"server certificate chain in PEM format, enables TLS");

		app.add_option("--tls-key", tls_key,
			"server private key in PEM format");

		app.add_option("--tls-ca", tls_ca,
			"CA bundle used to verify the destination server");

		app.add_flag("--insecure", insecure,
			"disable certificate verification for outgoing TLS");

		app.add_flag("--allow-plaintext", allow_plaintext,
			"allow unencrypted non-local connections");

		app.add_option("--token", token,
			"token sent to the destination server");

		app.add_option("--server-token", server_token,
			"token required from clients of this server");

		app.add_option("--max-connections", max_connections,
			"maximum simultaneous clients")->default_val("100");

		CLI11_PARSE(app, argc, argv);

		if (tls_cert.empty() != tls_key.empty()) {
			std::cout << "--tls-cert and --tls-key must be used together" << std::endl;
			return 1;
		}
		if (max_connections < 1) {
			std::cout << "--max-connections must be at least 1" << std::endl;
			return 1;
		}
#ifndef CHATPP_ENABLE_TLS
		if (!tls_cert.empty() || !tls_key.empty() || !tls_ca.empty() || insecure) {
			std::cout << "This build does not support TLS" << std::endl;
			return 1;
		}
#endif
		if (tls_cert.empty() && !allow_plaintext && !is_loopback(host)) {
			std::cout << "Refusing to expose an unencrypted server on a non-local address. "
				"Use --tls-cert/--tls-key or pass --allow-plaintext." << std::endl;
			return 1;
		}

		std::unique_ptr<chatpp::chat_server_base> cserver;
#ifdef CHATPP_ENABLE_TLS
		if (!tls_cert.empty()) {
			cserver = std::make_unique<chatpp::tls_chat_server>(tls_cert, tls_key);
		}
		else
#endif
		{
			cserver = std::make_unique<chatpp::chat_server>();
		}
		cserver->set_server_token(server_token);
		cserver->set_max_connections(static_cast<std::size_t>(max_connections));

		cserver->start(host, port);
		std::cout << rang::fg::yellow << "Listening on " << host << ":" <<
			port << std::endl << rang::fg::reset;

		std::string nickname;
		std::cout << rang::style::bold << rang::fg::blue << "Type your nickname : > " << rang::fg::reset;
		std::getline(std::cin, nickname);
		std::cout  << rang::style::bold << rang::fg::blue << "Type the destination URI : > " << rang::fg::reset;
		std::getline(std::cin, URI);

		bool use_tls = URI.rfind("wss://", 0) == 0;
		if (!use_tls && URI.rfind("ws://", 0) != 0) {
			std::cout << "Destination URI must start with ws:// or wss://" << std::endl;
			cserver->stop();
			return 1;
		}
		if (!use_tls && !allow_plaintext) {
			try {
				websocketpp::uri parsed(URI);
				if (!is_loopback(parsed.get_host())) {
					std::cout << "Refusing a plaintext connection to a non-local host. "
						"Use wss:// or pass --allow-plaintext." << std::endl;
					cserver->stop();
					return 1;
				}
			}
			catch (std::exception&) {
			}
		}

		std::unique_ptr<chatpp::chat_client_base> cclient;
		if (use_tls) {
#ifdef CHATPP_ENABLE_TLS
			auto client = std::make_unique<chatpp::tls_chat_client>();
			client->set_tls_options(tls_ca, insecure);
			cclient = std::move(client);
#else
			std::cout << "This build does not support TLS" << std::endl;
			cserver->stop();
			return 1;
#endif
		}
		else {
			cclient = std::make_unique<chatpp::chat_client>();
		}
		cclient->set_auth_token(token);

		cclient->start(URI);
		//wait some time until the connection is established.
		while (cclient->getStatus() == chatpp::chat_client_base::status::connecting) {
			std::this_thread::sleep_for(200ms);
		};

		if (cclient->getStatus() != chatpp::chat_client_base::status::open) {
			std::cout << rang::fg::red << "Connection failed : " <<
				cclient->getErrorMessage() << rang::fg::reset << std::endl;
			cclient->stop();
			cserver->stop();
			return 1;
		}

		cclient->send_nickname(nickname);

		std::mutex print_mutex;
		std::thread t([&]() { 
			while (auto item = cclient->msg_queue_.wait_pop_message()) {
				auto [nickname, msg] = *item;
				std::lock_guard<std::mutex> lk(print_mutex);
				std::cout << rang::fg::yellow << rang::style::bold << sanitize(nickname) <<
					" > " << sanitize(msg) << rang::fg::reset << std::endl;
			}	
		});

		try {
			std::string message;
			while (true) {
				{
					std::lock_guard<std::mutex> lk(print_mutex);
					std::cout << rang::style::bold << sanitize(nickname) << "> " << rang::fg::reset;
				}
				if (!std::getline(std::cin, message)) {
					break;
				}
				if (cclient->getStatus() != chatpp::chat_client_base::status::open) {
					break;
				}
				cclient->send(message);
			}
		}
		catch (...) {
			cclient->stop();
			cserver->stop();
			t.join();
			throw;
		}

		cclient->stop();
		cserver->stop();
		t.join();
	}
	catch (websocketpp::exception const& e) {
		std::cout << e.what() << std::endl;
		return 1;
	}
	catch (std::exception& e) {
		std::cout << "other exception : " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
