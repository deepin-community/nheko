#pragma once

/// @file
/// @brief Include for the @ref coeurl::Client

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <curl/curl.h>
#include <event2/event.h>
#include <event2/event_struct.h>
#include <spdlog/logger.h>

#include "headers.hpp"

/// @namespace coeurl
/// @brief Namespace for all public @ref coeurl classes
namespace coeurl {
struct Request;
struct SockInfo;

//! Global information, common to all connections
struct Client {
    //! construct a new client. For http/3 support, set the alt_svc_cache_path as well.
    Client();
    /// @brief cleans up a client
    /// Implicitly closes the connection and blocks until all of them exited.
    ~Client();

    //! Uncopyable
    Client(Client const &) = delete;
    //! Uncopyable
    void operator=(Client const &) = delete;
    //! Unmoveable
    Client(Client &&) = delete;
    //! Unmoveable
    void operator=(Client &&) = delete;

    //! Submit a manually created request
    void submit_request(std::shared_ptr<Request> conn);
    //! Stop all currently running requests
    void shutdown();
    //! Stop the event loop. If you force close it, all pending requests are
    //! cancelled.
    void close(bool force = false);

    /// @brief Make a simple GET request.
    /// For more complicated requests, create it manually and call submit_request.
    /// @param url The url to request.
    /// @param callback The callback, which will be called after the request is
    /// completed. The request will be passed as a parameter, which you can use
    /// to access the response.
    /// @param headers Headers to use for this request. Defaults to none.
    /// @param max_redirects How many redirects to follow. Defaults to none.
    void get(std::string url, std::function<void(const Request &)> callback, const Headers &headers = {},
             long max_redirects = 0);

    /// @brief Make a simple DELETE request.
    /// @param url The url to request.
    /// @param callback The callback, which will be called after the request is
    /// completed. The request will be passed as a parameter, which you can use
    /// to access the response.
    /// @param headers Headers to use for this request. Defaults to none.
    /// @param max_redirects How many redirects to follow. Defaults to none.
    void delete_(std::string url, std::function<void(const Request &)> callback, const Headers &headers = {},
                 long max_redirects = 0);

    /// @brief Make a simple DELETE request with a body.
    /// @param url The url to request.
    /// @param request_body The body to use with this request.
    /// @param mimetype The mimetype of the @a request_body.
    /// @param callback The callback, which will be called after the request is
    /// completed. The request will be passed as a parameter, which you can use
    /// to access the response.
    /// @param headers Headers to use for this request. Defaults to none.
    /// @param max_redirects How many redirects to follow. Defaults to none.
    void delete_(std::string url, std::string request_body, std::string mimetype,
                 std::function<void(const Request &)> callback, const Headers &headers = {}, long max_redirects = 0);

    /// @brief Make a simple HEAD request.
    /// For more complicated requests, create it manually and call submit_request.
    /// @param url The url to request.
    /// @param callback The callback, which will be called after the request is
    /// completed. The request will be passed as a parameter, which you can use
    /// to access the response.
    /// @param headers Headers to use for this request. Defaults to none.
    /// @param max_redirects How many redirects to follow. Defaults to none.
    void head(std::string url, std::function<void(const Request &)> callback, const Headers &headers = {},
              long max_redirects = 0);

    /// @brief Make a simple OPTIONS request.
    /// For more complicated requests, create it manually and call submit_request.
    /// @param url The url to request.
    /// @param callback The callback, which will be called after the request is
    /// completed. The request will be passed as a parameter, which you can use
    /// to access the response.
    /// @param headers Headers to use for this request. Defaults to none.
    /// @param max_redirects How many redirects to follow. Defaults to none.
    void options(std::string url, std::function<void(const Request &)> callback, const Headers &headers = {},
                 long max_redirects = 0);

    /// @brief Make a simple PUT request with a body.
    /// @param url The url to request.
    /// @param request_body The body to use with this request.
    /// @param mimetype The mimetype of the @a request_body.
    /// @param callback The callback, which will be called after the request is
    /// completed. The request will be passed as a parameter, which you can use
    /// to access the response.
    /// @param headers Headers to use for this request. Defaults to none.
    /// @param max_redirects How many redirects to follow. Defaults to none.
    void put(std::string url, std::string request_body, std::string mimetype,
             std::function<void(const Request &)> callback, const Headers &headers = {}, long max_redirects = 0);

    /// @brief Make a simple POST request with a body.
    /// @param url The url to request.
    /// @param request_body The body to use with this request.
    /// @param mimetype The mimetype of the @a request_body.
    /// @param callback The callback, which will be called after the request is
    /// completed. The request will be passed as a parameter, which you can use
    /// to access the response.
    /// @param headers Headers to use for this request. Defaults to none.
    /// @param max_redirects How many redirects to follow. Defaults to none.
    void post(std::string url, std::string request_body, std::string mimetype,
              std::function<void(const Request &)> callback, const Headers &headers = {}, long max_redirects = 0);

    /// @brief Set a global @a logger.
    /// @param logger The spdlog logger to use for logging.
    ///
    /// Set the logger while no requests are in flight, for example before
    /// starting the first request.
    static void set_logger(std::shared_ptr<spdlog::logger> logger) { log = std::move(logger); }
    //! Set whether to \a verify the https certificates or not.
    void set_verify_peer(bool verify) { this->verify_peer_ = verify; }
    /// @brief Query whether certificate verification is enabled or not
    /// @sa set_verify_peer
    bool does_verify_peer() { return this->verify_peer_; }

    //! Timeout connection after the specified amount of seconds, if the server
    //! stops sending acks.
    void connection_timeout(long t) { connection_timeout_ = t; }

    //! Maximum connections to open in parallel for this client. Set to 0 to disable the limit. Default is 64.
    void maximum_total_connections(long count);
    //! Maximum connections to open in parallel for each specific host for this client. Set to 0 to disable the limit.
    //! Default is 8.
    void maximum_connections_per_host(long count);

    //! Cache alternative service entries. This allows automatically switching to http/3.
    //! Usually you provide a path in `~/.cache` or similar.
    void alt_svc_cache_path(const std::string &path) { alt_svc_cache_path_ = path; }
    //! Where alternate svc entries are cached. Usually in `~/.cache`.
    const std::string &alt_svc_cache_path() const { return alt_svc_cache_path_; }

  private:
    // Call this to run the event loop. Will block until the client is shutdown.
    void run();

    void remove_request(Request *r);
    void check_multi_info();

    static void event_cb(evutil_socket_t fd, short kind, void *userp);
    static void timer_cb(evutil_socket_t fd, short kind, void *userp);
    static void add_pending_requests_cb(evutil_socket_t, short, void *userp);
    static void stop_ev_loop_cb(evutil_socket_t, short, void *userp);
    static void cancel_requests_cb(evutil_socket_t, short, void *userp);

    static int multi_timer_cb(CURLM *multi, long timeout_ms, Client *g);

    static int sock_cb(CURL *e, curl_socket_t s, int what, void *cbp, void *sockp);
    void addsock(curl_socket_t s, int action);
    void setsock(SockInfo *f, curl_socket_t s, int act);
    void remsock(SockInfo *f);

    static void mcode_or_die(const char *where, CURLMcode code);

    static std::shared_ptr<spdlog::logger> log;

    struct event_base *evbase{};
    struct event timer_event {};
    struct event add_request_timer {};
    struct event stop_event {};
    struct event cancel_requests_timer {};
    CURLM *multi{};
    int still_running = 0;
    std::atomic<bool> stopped{false};
    std::atomic<bool> prevent_new_requests{false};
    bool verify_peer_ = true;

    long connection_timeout_ = 0;

    std::mutex pending_requests_mutex;
    std::vector<std::shared_ptr<Request>> pending_requests;
    std::mutex running_requests_mutex;
    std::vector<std::shared_ptr<Request>> running_requests;
    std::mutex stopped_mutex;

    std::thread bg_thread;

    std::string alt_svc_cache_path_;

    friend Request;
};
} // namespace coeurl
