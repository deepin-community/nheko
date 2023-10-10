#include "coeurl/request.hpp"

#include "coeurl/client.hpp"

// for TCP_MAXRT
#if __has_include(<winsock2.h>)
#include <winsock2.h>
#endif

// for TCP_USER_TIMEOUT
#if __has_include(<netinet/tcp.h>)
#include <netinet/tcp.h>
#endif

namespace coeurl {
/* CURLOPT_WRITEFUNCTION */
size_t Request::write_cb(void *ptr, size_t size, size_t nmemb, void *data) {
    Request *request = (Request *)data;
    Client::log->trace("Write: {} ({})", request->url_, nmemb);
    request->response_.insert(request->response_.end(), (uint8_t *)ptr, (uint8_t *)ptr + nmemb);

    return size * nmemb;
}

/* CURLOPT_WRITEFUNCTION */
size_t Request::read_cb(char *buffer, size_t size, size_t nitems, void *data) {
    Request *request = (Request *)data;

    size_t data_left = request->request_.size() - request->readoffset;

    auto data_to_copy = std::min(data_left, nitems * size);

    Client::log->trace("Read: {} ({})", request->url_, data_to_copy);

    if (data_to_copy) {
        auto read_ptr = request->request_.data() + request->readoffset;
        Client::log->trace("Copying: {}", std::string_view(read_ptr, data_to_copy));
        std::copy(read_ptr, read_ptr + data_to_copy, buffer);
        Client::log->trace("Copied: {}", std::string_view(buffer, data_to_copy));

        request->readoffset += data_to_copy;
    }

    return data_to_copy;
}

static std::string_view trim(std::string_view val) {
    while (val.size() && isspace(val.front()))
        val.remove_prefix(1);
    while (val.size() && isspace(val.back()))
        val.remove_suffix(1);

    return val;
}

static char ascii_lower(char c) {
    if (c >= 'A' && c <= 'Z')
        c |= 0b00100000;
    return c;
}

bool header_less::operator()(const std::string &a, const std::string &b) const {
    if (a.size() != b.size())
        return a.size() < b.size();

    for (size_t i = 0; i < a.size(); i++) {
        auto a_c = ascii_lower(a[i]);
        auto b_c = ascii_lower(b[i]);

        if (a_c != b_c)
            return a_c < b_c;
    }

    return false;
}

/* CURLOPT_HEADERFUNCTION */
size_t Request::header_cb(char *buffer, size_t, size_t nitems, void *data) {
    Request *request = (Request *)data;
    std::string_view header(buffer, nitems);

    if (auto pos = header.find(':'); pos != std::string_view::npos) {
        auto key = trim(header.substr(0, pos));
        auto val = trim(header.substr(pos + 1));

        Client::log->debug("Header: {} ({}: {})", request->url_, key, val);

        request->response_headers_.insert({std::string(key), std::string(val)});
    }

    return nitems;
}

/* CURLOPT_PROGRESSFUNCTION */
int Request::prog_cb(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ult, curl_off_t uln) {
    Request *r = (Request *)p;
    (void)ult;
    (void)uln;

    if (r->on_upload_progress_)
        r->on_upload_progress_(uln, ult);
    if (r->on_download_progess_)
        r->on_download_progess_(dlnow, dltotal);

    Client::log->trace("Progress: {} ({}/{}):({}/{})", r->url_, uln, ult, dlnow, dltotal);
    return 0;
}

Request::Request(Client *client, Method m, std::string url__) : url_(std::move(url__)), global(client), method(m) {
    this->easy = curl_easy_init();
    if (!this->easy) {
        Client::log->critical("curl_easy_init() failed, exiting!");
        throw std::bad_alloc();
    }

    curl_easy_setopt(this->easy, CURLOPT_URL, this->url_.c_str());
    curl_easy_setopt(this->easy, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(this->easy, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(this->easy, CURLOPT_HEADERFUNCTION, header_cb);
    curl_easy_setopt(this->easy, CURLOPT_HEADERDATA, this);
    // curl_easy_setopt(this->easy, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(this->easy, CURLOPT_ERRORBUFFER, this->error);
    curl_easy_setopt(this->easy, CURLOPT_PRIVATE, this);
    curl_easy_setopt(this->easy, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(this->easy, CURLOPT_XFERINFOFUNCTION, prog_cb);
    curl_easy_setopt(this->easy, CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(this->easy, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);

    // enable altsvc support, which allows us to switch to http3
    curl_easy_setopt(this->easy, CURLOPT_ALTSVC_CTRL, CURLALTSVC_H1|CURLALTSVC_H2|CURLALTSVC_H3);
    curl_easy_setopt(this->easy, CURLOPT_ALTSVC, client->alt_svc_cache_path().c_str());

    // default to all supported encodings
    curl_easy_setopt(this->easy, CURLOPT_ACCEPT_ENCODING, "");

    switch (m) {
    case Method::Delete:
        curl_easy_setopt(this->easy, CURLOPT_HTTPGET, 0L);
        curl_easy_setopt(this->easy, CURLOPT_CUSTOMREQUEST, "DELETE");
        break;
    case Method::Get:
        curl_easy_setopt(this->easy, CURLOPT_HTTPGET, 1L);
        break;
    case Method::Head:
        curl_easy_setopt(this->easy, CURLOPT_NOBODY, 1L);
        break;
    case Method::Options:
        curl_easy_setopt(this->easy, CURLOPT_CUSTOMREQUEST, "OPTIONS");
        break;
    case Method::Patch:
        curl_easy_setopt(this->easy, CURLOPT_CUSTOMREQUEST, "PATCH");
        break;
    case Method::Post:
        curl_easy_setopt(this->easy, CURLOPT_POST, 1L);
        request("");
        break;
    case Method::Put:
        curl_easy_setopt(this->easy, CURLOPT_CUSTOMREQUEST, "PUT");
        request("");
        break;
    }

    verify_peer(this->global->does_verify_peer());
}

Request::~Request() {
    curl_easy_cleanup(this->easy);
    curl_slist_free_all(request_headers_);
}

Request &Request::max_redirects(long amount) {
    curl_easy_setopt(this->easy, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(this->easy, CURLOPT_MAXREDIRS, amount);
    return *this;
}

Request &Request::verify_peer(bool verify) {
    curl_easy_setopt(this->easy, CURLOPT_SSL_VERIFYPEER, verify ? 1L : 0L);
    return *this;
}

Request &Request::request(std::string r, std::string contenttype) {
    this->request_ = std::move(r);
    this->request_contenttype_ = std::move(contenttype);

    curl_easy_setopt(this->easy, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(request_.size()));
    curl_easy_setopt(this->easy, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(request_.size()));
    curl_easy_setopt(this->easy, CURLOPT_READDATA, this);
    curl_easy_setopt(this->easy, CURLOPT_READFUNCTION, read_cb);
    curl_easy_setopt(this->easy, CURLOPT_POSTFIELDS, nullptr);
    return *this;
}

Request &Request::request_headers(const Headers &h) {
    if (request_headers_)
        curl_slist_free_all(request_headers_);

    for (const auto &[k, v] : h) {
        request_headers_ = curl_slist_append(request_headers_, (k + ": " + v).c_str());
    }

    if (!request_contenttype_.empty())
        request_headers_ = curl_slist_append(request_headers_, ("content-type: " + request_contenttype_).c_str());

    curl_easy_setopt(this->easy, CURLOPT_HTTPHEADER, request_headers_);
    return *this;
}

Request &Request::connection_timeout(long t) {
    // only allow values that are > 0, if they are divided by 3
    if (t <= 2)
        return *this;

    // enable keepalive
    curl_easy_setopt(this->easy, CURLOPT_TCP_KEEPALIVE, 1L);

    // send keepalives every third of the timeout interval. This allows for
    // retransmission of 2 keepalive while still giving the server a third of the
    // time to reply
    curl_easy_setopt(this->easy, CURLOPT_TCP_KEEPIDLE, t / 3);
    curl_easy_setopt(this->easy, CURLOPT_TCP_KEEPINTVL, t / 3);

    this->connection_timeout_ = t;
#ifdef TCP_USER_TIMEOUT
    // The + is needed to convert this to a function pointer!
    curl_easy_setopt(
        this->easy, CURLOPT_SOCKOPTFUNCTION, +[](void *clientp, curl_socket_t curlfd, curlsocktype) -> int {
            unsigned int val = static_cast<Request *>(clientp)->connection_timeout_ * 1000 /*ms*/;
            setsockopt(curlfd, SOL_TCP, TCP_USER_TIMEOUT, (const char *)&val, sizeof(val));
            return CURLE_OK;
        });
#elif defined(TCP_MAXRT)
    // The + is needed to convert this to a function pointer!
    curl_easy_setopt(
        this->easy, CURLOPT_SOCKOPTFUNCTION, +[](void *clientp, curl_socket_t sock, curlsocktype) -> int {
            unsigned int maxrt = static_cast<Request *>(clientp)->connection_timeout_ /*s*/;
            setsockopt(sock, IPPROTO_TCP, TCP_MAXRT, (const char *)&maxrt, sizeof(maxrt));
            return CURLE_OK;
        });
#endif
    curl_easy_setopt(this->easy, CURLOPT_SOCKOPTDATA, this);

    return *this;
}

Request &Request::on_complete(std::function<void(const Request &)> handler) {
    on_complete_ = handler;
    return *this;
}

Request &Request::on_upload_progress(std::function<void(size_t progress, size_t total)> handler) {
    on_upload_progress_ = handler;
    curl_easy_setopt(this->easy, CURLOPT_NOPROGRESS, 0L);
    return *this;
}
Request &Request::on_download_progess(std::function<void(size_t progress, size_t total)> handler) {
    on_download_progess_ = handler;
    curl_easy_setopt(this->easy, CURLOPT_NOPROGRESS, 0L);
    return *this;
}

int Request::response_code() const {
    long http_code;
    curl_easy_getinfo(const_cast<CURL *>(this->easy), CURLINFO_RESPONSE_CODE, &http_code);
    return static_cast<int>(http_code);
}
} // namespace coeurl
