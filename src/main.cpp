#include "rang.hpp"
#include <chrono>
#include <thread>
#include "queue.hpp"
#include "client.hpp"
#include "server.hpp"
#include "CLI11.hpp"
#include "rsa.hpp"

using namespace std::chrono_literals;

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

		int d = -1;
		int port = 9002;
		std::string URI;

		//add configuration file
		app.set_config("--conf")->default_val("conf.ini");

		app.add_option("-p,--port", port,
			"specify the listening port")->default_val("9002");

		CLI11_PARSE(app, argc, argv);

		chatpp::chat_server cserver;
		chatpp::chat_client cclient;

		cserver.start(port);
		std::cout << rang::fg::yellow << "Listening on port : " << port << std::endl << rang::fg::reset;


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

		cclient.send_nickname(nickname);
		
		std::thread t([&]() { 
			while (auto item = cserver.msg_queue_.wait_pop_message()) {
				auto [nickname, msg] = *item;
				std::cout << rang::fg::yellow << rang::style::bold << nickname <<
					" > " << msg << rang::fg::reset << std::endl;
			}	
		});
		t.detach();

		std::string message;
		while (true) {
			std::cout  << rang::style::bold << nickname << "> " << rang::fg::reset;
			std::getline(std::cin, message);
			cclient.send(message);
		}
	}
	catch (websocketpp::exception const& e) {
		std::cout << e.what() << std::endl;
	}
	catch (std::exception& e) {
		std::cout << "other exception : " << e.what() << std::endl;
	}

	//chat_server.stop();
}
