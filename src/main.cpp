#include "rang.hpp"
#include <chrono>
#include <thread>
#include "queue.hpp"
#include "client.hpp"
#include "server.hpp"
#include "CLI11.hpp"
#include "rsa.hpp"

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

		//add configuration file
		app.set_config("--conf")->default_val("conf.ini");

		app.add_option("-H,--host", host,
			"specify the listening address")->default_val("127.0.0.1");

		app.add_option("-p,--port", port,
			"specify the listening port")->default_val("9002");

		CLI11_PARSE(app, argc, argv);

		chatpp::chat_server cserver;
		chatpp::chat_client cclient;

		cserver.start(host, port);
		std::cout << rang::fg::yellow << "Listening on " << host << ":" <<
			port << std::endl << rang::fg::reset;


		std::string nickname;
		std::cout << rang::style::bold << rang::fg::blue << "Type your nickname : > " << rang::fg::reset;
		std::getline(std::cin, nickname);
		std::cout  << rang::style::bold << rang::fg::blue << "Type the destination URI : > " << rang::fg::reset;
		std::getline(std::cin, URI);
		cclient.start(URI);
		//wait some time until the connection is established.
		while (cclient.getStatus() == chatpp::chat_client::status::connecting) {
			std::this_thread::sleep_for(200ms);
		};

		if (cclient.getStatus() != chatpp::chat_client::status::open) {
			std::cout << rang::fg::red << "Connection failed : " <<
				cclient.getErrorMessage() << rang::fg::reset << std::endl;
			cclient.stop();
			cserver.stop();
			return 1;
		}

		cclient.send_nickname(nickname);
		
		std::thread t([&]() { 
			while (auto item = cclient.msg_queue_.wait_pop_message()) {
				auto [nickname, msg] = *item;
				std::cout << rang::fg::yellow << rang::style::bold << sanitize(nickname) <<
					" > " << sanitize(msg) << rang::fg::reset << std::endl;
			}	
		});

		std::string message;
		while (true) {
			std::cout  << rang::style::bold << sanitize(nickname) << "> " << rang::fg::reset;
			if (!std::getline(std::cin, message)) {
				break;
			}
			if (cclient.getStatus() != chatpp::chat_client::status::open) {
				break;
			}
			cclient.send(message);
		}

		cclient.stop();
		cserver.stop();
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
