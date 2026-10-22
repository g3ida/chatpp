#include "tls.hpp"

#ifdef CHATPP_ENABLE_TLS

using namespace chatpp;

namespace {
	void set_modern_options(asio::ssl::context& context) {
		context.set_options(
			asio::ssl::context::default_workarounds |
			asio::ssl::context::no_sslv2 |
			asio::ssl::context::no_sslv3 |
			asio::ssl::context::no_tlsv1 |
			asio::ssl::context::no_tlsv1_1 |
			asio::ssl::context::single_dh_use);
	}
}

tls_context_ptr chatpp::make_server_tls_context(const std::string& cert_file, const std::string& key_file) {
	auto context = std::make_shared<asio::ssl::context>(asio::ssl::context::tls_server);
	set_modern_options(*context);
	context->use_certificate_chain_file(cert_file);
	context->use_private_key_file(key_file, asio::ssl::context::pem);
	return context;
}

tls_context_ptr chatpp::make_client_tls_context(const std::string& host, const std::string& ca_file, bool insecure) {
	auto context = std::make_shared<asio::ssl::context>(asio::ssl::context::tls_client);
	set_modern_options(*context);
	if (insecure) {
		context->set_verify_mode(asio::ssl::verify_none);
		return context;
	}
	context->set_verify_mode(asio::ssl::verify_peer);
	if (!ca_file.empty()) {
		context->load_verify_file(ca_file);
	}
	else {
		context->set_default_verify_paths();
	}
	context->set_verify_callback(asio::ssl::host_name_verification(host));
	return context;
}

#endif
