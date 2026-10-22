#ifndef CHATPP_TLS_HPP
#define CHATPP_TLS_HPP

#ifdef CHATPP_ENABLE_TLS

#include <asio/ssl.hpp>

#include <memory>
#include <string>

namespace chatpp {
	using tls_context_ptr = std::shared_ptr<asio::ssl::context>;

	tls_context_ptr make_server_tls_context(const std::string& cert_file, const std::string& key_file);
	tls_context_ptr make_client_tls_context(const std::string& host, const std::string& ca_file, bool insecure);
}

#endif

#endif
