/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibProtocol/Request.h>
#include <LibProtocol/RequestClient.h>

namespace Protocol {

RequestClient::RequestClient(NonnullOwnPtr<Core::LocalSocket> socket)
    : IPC::ConnectionToServer<RequestClientEndpoint, RequestServerEndpoint>(*this, move(socket))
{
}

void RequestClient::ensure_connection(URL const& url, ::RequestServer::CacheLevel cache_level)
{
    async_ensure_connection(url, cache_level);
}

template<typename RequestHashMapTraits>
RefPtr<Request> RequestClient::start_request(ByteString const& method, URL const& url, HashMap<ByteString, ByteString, RequestHashMapTraits> const& request_headers, ReadonlyBytes request_body, Core::ProxyData const& proxy_data)
{
    auto headers_or_error = request_headers.template clone<Traits<ByteString>>();
    if (headers_or_error.is_error())
        return nullptr;
    auto body_result = ByteBuffer::copy(request_body);
    if (body_result.is_error())
        return nullptr;

    static i32 s_next_request_id = 0;
    auto request_id = s_next_request_id++;

    IPCProxy::async_start_request(request_id, method, url, headers_or_error.release_value(), body_result.release_value(), proxy_data);
    auto request = Request::create_from_id({}, *this, request_id);
    m_requests.set(request_id, request);
    return request;
}

void RequestClient::request_started(i32 request_id, IPC::File const& response_file)
{
    auto request = m_requests.get(request_id);
    if (!request.has_value()) {
        warnln("Received response for non-existent request {}", request_id);
        return;
    }

    auto response_fd = response_file.take_fd();
    request.value()->set_request_fd({}, response_fd);
}

bool RequestClient::stop_request(Badge<Request>, Request& request)
{
    if (!m_requests.contains(request.id()))
        return false;
    return IPCProxy::stop_request(request.id());
}

bool RequestClient::set_certificate(Badge<Request>, Request& request, ByteString certificate, ByteString key)
{
    if (!m_requests.contains(request.id()))
        return false;
    return IPCProxy::set_certificate(request.id(), move(certificate), move(key));
}

void RequestClient::request_finished(i32 request_id, bool success, u64 total_size)
{
    RefPtr<Request> request;
    if ((request = m_requests.get(request_id).value_or(nullptr))) {
        request->did_finish({}, success, total_size);
    }
    m_requests.remove(request_id);
}

void RequestClient::request_progress(i32 request_id, Optional<u64> const& total_size, u64 downloaded_size)
{
    if (auto request = const_cast<Request*>(m_requests.get(request_id).value_or(nullptr))) {
        request->did_progress({}, total_size, downloaded_size);
    }
}

void RequestClient::headers_became_available(i32 request_id, HashMap<ByteString, ByteString, CaseInsensitiveStringTraits> const& response_headers, Optional<u32> const& status_code)
{
    auto request = const_cast<Request*>(m_requests.get(request_id).value_or(nullptr));
    if (!request) {
        warnln("Received headers for non-existent request {}", request_id);
        return;
    }
    auto response_headers_clone_or_error = response_headers.clone();
    if (response_headers_clone_or_error.is_error()) {
        warnln("Error while receiving headers for request {}: {}", request_id, response_headers_clone_or_error.error());
        return;
    }

    request->did_receive_headers({}, response_headers_clone_or_error.release_value(), status_code);
}

void RequestClient::certificate_requested(i32 request_id)
{
    if (auto request = const_cast<Request*>(m_requests.get(request_id).value_or(nullptr))) {
        request->did_request_certificates({});
    }
}

RefPtr<WebSocket> RequestClient::websocket_connect(const URL& url, ByteString const& origin, Vector<ByteString> const& protocols, Vector<ByteString> const& extensions, HashMap<ByteString, ByteString> const& request_headers)
{
    auto headers_or_error = request_headers.clone();
    if (headers_or_error.is_error())
        return nullptr;
    auto connection_id = IPCProxy::websocket_connect(url, origin, protocols, extensions, headers_or_error.release_value());
    if (connection_id < 0)
        return nullptr;
    auto connection = WebSocket::create_from_id({}, *this, connection_id);
    m_websockets.set(connection_id, connection);
    return connection;
}

void RequestClient::websocket_connected(i32 connection_id)
{
    auto maybe_connection = m_websockets.get(connection_id);
    if (maybe_connection.has_value())
        maybe_connection.value()->did_open({});
}

void RequestClient::websocket_received(i32 connection_id, bool is_text, ByteBuffer const& data)
{
    auto maybe_connection = m_websockets.get(connection_id);
    if (maybe_connection.has_value())
        maybe_connection.value()->did_receive({}, data, is_text);
}

void RequestClient::websocket_errored(i32 connection_id, i32 message)
{
    auto maybe_connection = m_websockets.get(connection_id);
    if (maybe_connection.has_value())
        maybe_connection.value()->did_error({}, message);
}

void RequestClient::websocket_closed(i32 connection_id, u16 code, ByteString const& reason, bool clean)
{
    auto maybe_connection = m_websockets.get(connection_id);
    if (maybe_connection.has_value())
        maybe_connection.value()->did_close({}, code, reason, clean);
}

void RequestClient::websocket_certificate_requested(i32 connection_id)
{
    auto maybe_connection = m_websockets.get(connection_id);
    if (maybe_connection.has_value())
        maybe_connection.value()->did_request_certificates({});
}

}

template RefPtr<Protocol::Request> Protocol::RequestClient::start_request(ByteString const& method, URL const&, HashMap<ByteString, ByteString> const& request_headers, ReadonlyBytes request_body, Core::ProxyData const&);
template RefPtr<Protocol::Request> Protocol::RequestClient::start_request(ByteString const& method, URL const&, HashMap<ByteString, ByteString, CaseInsensitiveStringTraits> const& request_headers, ReadonlyBytes request_body, Core::ProxyData const&);
