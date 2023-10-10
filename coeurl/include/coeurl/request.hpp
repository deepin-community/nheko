#pragma once

/// @file
/// @brief Include for the @ref coeurl::Request

#include <functional>
#include <string>

#include <curl/curl.h>

#include "headers.hpp"

namespace coeurl {
struct Client;

/// @brief A HTTP request.
///
/// Can be sent using a @ref Client
/// You can listen to various events here and access the response, after it
/// finished.
struct Request {
    //! The different HTTP methods.
    enum class Method {
        Delete,  //!< HTTP DELETE
        Get,     //!< HTTP GET
        Head,    //!< HTTP HEAD
        Options, //!< HTTP OPTIONS
        Patch,   //!< HTTP PATCH
        Post,    //!< HTTP POST
        Put,     //!< HTTP PUT
    };

    /// @brief construct a new request to @a url.
    /// @param url The URL to request.
    /// @param client The client to use.
    /// @param method The http method for this request. Usually GET.
    Request(Client *client, Method method, std::string url);
    //! Cleans up a request.
    ~Request();
    //! Uncopyable
    Request(Request const &) = delete;
    //! Uncopyable
    void operator=(Request const &) = delete;
    //! Unmoveable
    Request(Request &&) = delete;
    //! Unmoveable
    void operator=(Request &&) = delete;

    //! The maximum number of redirects. Defaults to 0.
    Request &max_redirects(long amount);
    //! Whether to verify the certificate of the peer. Defaults to whatever is
    //! set on the client (usually true).
    Request &verify_peer(bool verify);
    //! The actual request body. \a contenttype defaults to
    //! "application/octet-stream".
    Request &request(std::string r, std::string contenttype = "application/octet-stream");
    /// @brief Headers for this request.
    /// @sa request_headers
    Request &request_headers(const Headers &h);
    //! Timeout connection after the specified amount of seconds, if the server
    //! stops sending acks.
    Request &connection_timeout(long t);

    //! Optional completion handler.
    Request &on_complete(const std::function<void(const Request &)> handler);
    //! Optional upload progress handler.
    Request &on_upload_progress(std::function<void(size_t progress, size_t total)> handler);
    //! Optional download progress handler.
    Request &on_download_progess(std::function<void(size_t progress, size_t total)> handler);

    //! The url for this request.
    std::string_view url() const { return url_; }
    //! The response in this request.
    std::string_view response() const { return response_; }
    //! The HTTP response code. 200 for success.
    int response_code() const;
    //! The curl error code. CURLE_OK (0) on success.
    CURLcode error_code() const { return curl_error; }
    /// @brief Headers for the response
    /// @sa request_headers
    Headers response_headers() const { return response_headers_; }

  private:
    /* CURLOPT_WRITEFUNCTION */
    static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *data);
    /* CURLOPT_READFUNCTION */
    static size_t read_cb(char *buffer, size_t size, size_t nitems, void *data);
    /* CURLOPT_HEADERFUNCTION */
    static size_t header_cb(char *buffer, size_t size, size_t nitems, void *data);
    /* CURLOPT_PROGRESSFUNCTION */
    static int prog_cb(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ult, curl_off_t uln);

    CURL *easy;
    std::string request_;
    std::string request_contenttype_;
    std::string response_;
    std::string url_;
    Headers response_headers_;
    curl_slist *request_headers_ = nullptr;

    Client *global;
    size_t readoffset = 0;
    char error[CURL_ERROR_SIZE];

    enum class Status {
        Running,
        Canceled,
        Done,
    } status = Status::Running;
    CURLcode curl_error = CURLcode::CURLE_OK;
    Method method = Method::Get;

    long connection_timeout_ = 0;

    std::function<void(const Request &)> on_complete_;
    std::function<void(size_t progress, size_t total)> on_upload_progress_, on_download_progess_;

    friend Client;
};
} // namespace coeurl
