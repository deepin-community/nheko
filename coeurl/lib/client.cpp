#include "coeurl/client.hpp"

#include <event2/thread.h>
#include <spdlog/sinks/null_sink.h>

#include <thread>

#include "coeurl/request.hpp"

namespace coeurl {
std::shared_ptr<spdlog::logger> Client::log = spdlog::null_logger_mt("coeurl_null");

/* Die if we get a bad CURLMcode somewhere */
void Client::mcode_or_die(const char *where, CURLMcode code) {
    if (CURLM_OK != code) {
        const char *s = curl_multi_strerror(code);
        switch (code) {
        case CURLM_BAD_SOCKET:
            Client::log->error("{} returns {}", where, s);
            /* ignore this error */
            return;
        case CURLM_BAD_HANDLE:
        case CURLM_BAD_EASY_HANDLE:
        case CURLM_OUT_OF_MEMORY:
        case CURLM_INTERNAL_ERROR:
        case CURLM_UNKNOWN_OPTION:
        case CURLM_LAST:
            break;
        default:
            s = "CURLM_unknown";
            break;
        }
        Client::log->critical("{} returns {}", where, s);
        throw std::runtime_error(s);
    }
}

/* Information associated with a specific socket */
struct SockInfo {
    curl_socket_t sockfd;
    struct event ev;
};

/* Update the event timer after curl_multi library calls */
int Client::multi_timer_cb(CURLM *multi, long timeout_ms, Client *g) {
    struct timeval timeout;
    (void)multi;

    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    Client::log->trace("multi_timer_cb: Setting timeout to {} ms", timeout_ms);

    /*
     * if timeout_ms is -1, just delete the timer
     *
     * For all other values of timeout_ms, this should set or *update* the timer
     * to the new value
     */
    if (timeout_ms == -1)
        event_del(&g->timer_event);
    else /* includes timeout zero */ {
        event_add(&g->timer_event, &timeout);
    }
    return 0;
}

/* Called by libevent when we get action on a multi socket */
void Client::event_cb(evutil_socket_t fd, short kind, void *userp) {
    Client *g = (Client *)userp;

    int action = ((kind & EV_READ) ? CURL_CSELECT_IN : 0) | ((kind & EV_WRITE) ? CURL_CSELECT_OUT : 0);

    CURLMcode rc = curl_multi_socket_action(g->multi, fd, action, &g->still_running);
    mcode_or_die("event_cb: curl_multi_socket_action", rc);

    g->check_multi_info();
    if (g->still_running <= 0 && g->running_requests.empty()) {
        Client::log->trace("last transfer done, kill timeout");
        if (evtimer_pending(&g->timer_event, NULL)) {
            evtimer_del(&g->timer_event);
        }
    }
}

/* Called by libevent when our timeout expires */
void Client::timer_cb(evutil_socket_t, short, void *userp) {
    Client::log->trace("timer_cb");

    Client *g = (Client *)userp;

    CURLMcode rc = curl_multi_socket_action(g->multi, CURL_SOCKET_TIMEOUT, 0, &g->still_running);
    mcode_or_die("timer_cb: curl_multi_socket_action", rc);
    g->check_multi_info();
}

// Invoked when we were told to shut down.
void Client::stop_ev_loop_cb(evutil_socket_t, short, void *userp) {
    Client::log->trace("stop_ev_loop_cb");

    Client *g = (Client *)userp;

    CURLMcode rc = curl_multi_socket_action(g->multi, CURL_SOCKET_TIMEOUT, 0, &g->still_running);
    mcode_or_die("stop_ev_loop_cb: curl_multi_socket_action", rc);
    g->check_multi_info();
}

/* Called by libevent when our timeout expires */
void Client::add_pending_requests_cb(evutil_socket_t, short, void *userp) {
    Client::log->trace("add_pending_requests_cb");

    Client *g = (Client *)userp;

    {
        const std::scoped_lock lock(g->pending_requests_mutex, g->running_requests_mutex);

        for (size_t i = 0; i < g->pending_requests.size(); i++) {
            const auto &conn = g->pending_requests[i];

            Client::log->trace("Adding easy {} to multi {} ({})", conn->easy, g->multi, conn->url_.c_str());
            auto rc = curl_multi_add_handle(g->multi, conn->easy);
            mcode_or_die("new_conn: curl_multi_add_handle", rc);

            g->running_requests.push_back(std::move(g->pending_requests[i]));
        }

        g->pending_requests.clear();
    }
}

/* Called by libevent when our timeout expires */
void Client::cancel_requests_cb(evutil_socket_t, short, void *userp) {
    Client::log->trace("cancel_requests_cb");

    Client *g = (Client *)userp;

        // prevent new requests from being added
        {
            g->prevent_new_requests = true;
        }

        // safe to access now, since we are running on the worker thread and only
        // there running_requests is modified
        while (!g->running_requests.empty())
            g->remove_request(g->running_requests.back().get());

        // Allow for new requests
        {
            g->prevent_new_requests = false;
        }


    CURLMcode rc = curl_multi_socket_action(g->multi, CURL_SOCKET_TIMEOUT, 0, &g->still_running);
    mcode_or_die("timer_cb: curl_multi_socket_action", rc);
    g->check_multi_info();
}

/* Clean up the SockInfo structure */
void Client::remsock(SockInfo *f) {
    if (f) {
        if (event_initialized(&f->ev)) {
            event_del(&f->ev);
        }
        delete f;
    }
}

/* Assign information to a SockInfo structure */
void Client::setsock(SockInfo *f, curl_socket_t s, int act) {
    short kind = ((act & CURL_POLL_IN) ? EV_READ : 0) | ((act & CURL_POLL_OUT) ? EV_WRITE : 0) | EV_PERSIST;

    f->sockfd = s;
    if (event_initialized(&f->ev)) {
        event_del(&f->ev);
    }
    event_assign(&f->ev, this->evbase, f->sockfd, kind, event_cb, this);
    event_add(&f->ev, NULL);
}

/* Initialize a new SockInfo structure */
void Client::addsock(curl_socket_t s, int action) {
    SockInfo *fdp = new SockInfo();

    setsock(fdp, s, action);
    curl_multi_assign(this->multi, s, fdp);
}

/* CURLMOPT_SOCKETFUNCTION */
int Client::sock_cb(CURL *e, curl_socket_t s, int what, void *cbp, void *sockp) {
    Client *g = (Client *)cbp;
    SockInfo *fdp = (SockInfo *)sockp;
    const char *whatstr[] = {"none", "IN", "OUT", "INOUT", "REMOVE"};

    Client::log->trace("socket callback: s={} e={} what={} ", s, e, whatstr[what]);
    if (what == CURL_POLL_REMOVE) {
        g->remsock(fdp);
    } else {
        if (!fdp) {
            Client::log->trace("Adding data: {}", whatstr[what]);
            g->addsock(s, what);
        } else {
            Client::log->trace("Changing action to: {}", whatstr[what]);
            g->setsock(fdp, s, what);
        }
    }
    return 0;
}

Client::Client() {
    std::once_flag threads_once;
#ifdef WIN32
    std::call_once(threads_once, evthread_use_windows_threads);
#elif defined(EVENT__HAVE_PTHREADS)
    std::call_once(threads_once, evthread_use_pthreads);
#else
#error "No supported threading backend!"
#endif

    /* Make sure the SSL or WinSock backends are initialized */
    std::once_flag curl_once;
    std::call_once(curl_once, curl_global_init, CURL_GLOBAL_DEFAULT);

    this->evbase = event_base_new();
    this->multi = curl_multi_init();
    event_assign(&this->timer_event, this->evbase, -1, 0, timer_cb, this);
    event_assign(&this->add_request_timer, this->evbase, -1, 0, add_pending_requests_cb, this);
    event_assign(&this->stop_event, this->evbase, -1, 0, stop_ev_loop_cb, this);
    event_assign(&this->cancel_requests_timer, this->evbase, -1, 0, cancel_requests_cb, this);

    /* setup the generic multi interface options we want */
    curl_multi_setopt(this->multi, CURLMOPT_SOCKETFUNCTION, sock_cb);
    curl_multi_setopt(this->multi, CURLMOPT_SOCKETDATA, this);
    curl_multi_setopt(this->multi, CURLMOPT_TIMERFUNCTION, multi_timer_cb);
    curl_multi_setopt(this->multi, CURLMOPT_TIMERDATA, this);

    maximum_total_connections(64);
    maximum_connections_per_host(8);

    bg_thread = std::thread([this]() { this->run(); });
}

void Client::maximum_total_connections(long count) {
    curl_multi_setopt(this->multi, CURLMOPT_MAX_TOTAL_CONNECTIONS, count);
}

void Client::maximum_connections_per_host(long count) {
    curl_multi_setopt(this->multi, CURLMOPT_MAX_HOST_CONNECTIONS, count);
}

Client::~Client() {
    close();

    event_del(&this->timer_event);
    event_del(&this->add_request_timer);
    event_del(&this->stop_event);
    event_del(&this->cancel_requests_timer);
    event_base_free(this->evbase);
    curl_multi_cleanup(this->multi);
}

void Client::close(bool force) {
    std::unique_lock l{stopped_mutex};
    if (stopped)
        return;

    Client::log->trace("STOP");

    if (force)
        shutdown();

    stopped = true;
    event_active(&this->stop_event, 0, 0);

    Client::log->trace("WAITING");
    if (bg_thread.get_id() != std::this_thread::get_id())
        bg_thread.join();
    else
        bg_thread.detach();
    Client::log->trace("CLOSED");
}

void Client::shutdown() { event_active(&this->cancel_requests_timer, 0, 0); }

void Client::run() { event_base_loop(this->evbase, EVLOOP_NO_EXIT_ON_EMPTY); }

/* Check for completed transfers, and remove their easy handles */
void Client::check_multi_info() {
    CURLMsg *msg;
    int msgs_left;

    Client::log->trace("REMAINING: {}", this->still_running);
    while ((msg = curl_multi_info_read(this->multi, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE) {
            CURL *easy = msg->easy_handle;

            Request *conn;
            curl_easy_getinfo(easy, CURLINFO_PRIVATE, &conn);
            conn->status = Request::Status::Done;
            conn->curl_error = msg->data.result;

            remove_request(conn);
        }
    }

    if (this->still_running == 0)
        add_pending_requests_cb(0, 0, this);

    if (this->still_running == 0 && this->running_requests.empty() && this->stopped) {
        event_base_loopbreak(this->evbase);
        Client::log->trace("BREAK");
    }
    Client::log->trace("after check_multi_info: {}", this->still_running);
}

void Client::submit_request(std::shared_ptr<Request> conn) {
    Client::log->trace("SUBMIT");

    if (this->prevent_new_requests) {
        conn->curl_error = CURLE_ABORTED_BY_CALLBACK;
        conn->status = Request::Status::Canceled;
        if (conn->on_complete_)
            conn->on_complete_(*conn.get());
        return;
    }

    {
        const std::scoped_lock lock(pending_requests_mutex);

        pending_requests.push_back(conn);
    }

    event_active(&add_request_timer, 0, 0);
}
void Client::remove_request(Request *r) {
    Client::log->trace("REMOVE");

    std::shared_ptr<Request> req;

    {
        std::scoped_lock lock(this->running_requests_mutex);
        curl_multi_remove_handle(this->multi, r->easy);

        for (auto it = this->running_requests.begin(); this->running_requests.end() != it; ++it) {
            if (it->get() == r) {
                req = std::move(*it);
                this->running_requests.erase(it);
                break;
            }
        }
    }

    if (req) {
        long http_code;
        curl_easy_getinfo(req->easy, CURLINFO_RESPONSE_CODE, &http_code);

        Client::log->trace("DONE: {} => {} ({}) http: {}", req->url_, req->curl_error, req->error, http_code);

        if (req->on_complete_)
            req->on_complete_(*req.get());
    }
}

void Client::get(std::string url, std::function<void(const Request &)> callback, const Headers &headers,
                 long max_redirects) {
    auto req = std::make_shared<Request>(this, Request::Method::Get, std::move(url));

    req->on_complete(std::move(callback));

    if (!headers.empty())
        req->request_headers(headers);

    if (max_redirects > 0)
        req->max_redirects(max_redirects);

    req->connection_timeout(connection_timeout_);

    this->submit_request(std::move(req));
}

void Client::delete_(std::string url, std::function<void(const Request &)> callback, const Headers &headers,
                     long max_redirects) {
    auto req = std::make_shared<Request>(this, Request::Method::Delete, std::move(url));

    req->on_complete(std::move(callback));

    if (!headers.empty())
        req->request_headers(headers);

    if (max_redirects > 0)
        req->max_redirects(max_redirects);

    req->connection_timeout(connection_timeout_);

    this->submit_request(std::move(req));
}

void Client::delete_(std::string url, std::string request_body, std::string mimetype, std::function<void(const Request &)> callback, const Headers &headers,
                     long max_redirects) {
    auto req = std::make_shared<Request>(this, Request::Method::Delete, std::move(url));

    req->request(request_body, mimetype);
    req->on_complete(std::move(callback));

    if (!headers.empty())
        req->request_headers(headers);

    if (max_redirects > 0)
        req->max_redirects(max_redirects);

    req->connection_timeout(connection_timeout_);

    this->submit_request(std::move(req));
}

void Client::head(std::string url, std::function<void(const Request &)> callback, const Headers &headers,
                  long max_redirects) {
    auto req = std::make_shared<Request>(this, Request::Method::Head, std::move(url));

    req->on_complete(std::move(callback));

    if (!headers.empty())
        req->request_headers(headers);

    if (max_redirects > 0)
        req->max_redirects(max_redirects);

    req->connection_timeout(connection_timeout_);

    this->submit_request(std::move(req));
}

void Client::options(std::string url, std::function<void(const Request &)> callback, const Headers &headers,
                     long max_redirects) {
    auto req = std::make_shared<Request>(this, Request::Method::Options, std::move(url));

    req->on_complete(std::move(callback));

    if (!headers.empty())
        req->request_headers(headers);

    if (max_redirects > 0)
        req->max_redirects(max_redirects);

    req->connection_timeout(connection_timeout_);

    this->submit_request(std::move(req));
}

void Client::put(std::string url, std::string request_body, std::string mimetype,
                 std::function<void(const Request &)> callback, const Headers &headers, long max_redirects) {
    auto req = std::make_shared<Request>(this, Request::Method::Put, std::move(url));

    req->request(request_body, mimetype);
    req->on_complete(std::move(callback));

    if (!headers.empty())
        req->request_headers(headers);

    if (max_redirects > 0)
        req->max_redirects(max_redirects);

    req->connection_timeout(connection_timeout_);

    this->submit_request(std::move(req));
}

void Client::post(std::string url, std::string request_body, std::string mimetype,
                  std::function<void(const Request &)> callback, const Headers &headers, long max_redirects) {
    auto req = std::make_shared<Request>(this, Request::Method::Post, std::move(url));

    req->request(request_body, mimetype);
    req->on_complete(std::move(callback));

    if (!headers.empty())
        req->request_headers(headers);

    if (max_redirects > 0)
        req->max_redirects(max_redirects);

    req->connection_timeout(connection_timeout_);

    this->submit_request(std::move(req));
}
} // namespace coeurl
