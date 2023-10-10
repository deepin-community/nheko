#include <thread>

#include "coeurl/client.hpp"
#include "coeurl/request.hpp"
#include "curl/curl.h"

#if __has_include(<doctest.h>)
#include <doctest.h>
#else
#include <doctest/doctest.h>
#endif

using namespace coeurl;

TEST_CASE("Basic request") {
    Client g{};
    g.set_verify_peer(false);

    g.get("https://localhost:5443/", [](const Request &r) {
        CHECK(r.url() == "https://localhost:5443/");
        CHECK(r.response_code() == 200);
        CHECK(r.error_code() == CURLE_OK);
        CHECK(r.response() == "OK");
        CHECK(r.response_headers()["content-type"] == "text/plain; charset=utf-8");
    });
}

TEST_CASE("Basic manual request") {
    Client g{};
    g.set_verify_peer(false);

    auto r = std::make_shared<Request>(&g, Request::Method::Get, "https://localhost:5443/");
    r->on_complete([](const Request &r) {
        CHECK(r.url() == "https://localhost:5443/");
        CHECK(r.response_code() == 200);
        CHECK(r.response() == "OK");
        CHECK(r.response_headers()["content-type"] == "text/plain; charset=utf-8");
    });

    g.submit_request(r);
}

TEST_CASE("Basic redirect") {
    Client g{};
    g.set_verify_peer(false);

    g.get(
        "https://localhost:5443/redirect",
        [](const Request &r) {
            CHECK(r.url() == "https://localhost:5443/redirect");
            CHECK(r.response_code() == 200);
            CHECK(r.response() == "OK");
        },
        {}, 1);
}

TEST_CASE("No redirect") {
    Client g{};
    g.set_verify_peer(false);

    g.get(
        "https://localhost:5443/redirect",
        [](const Request &r) {
            CHECK(r.url() == "https://localhost:5443/redirect");
            CHECK(r.response_code() == 302);
        },
        {}, 0);
}

TEST_CASE("Max redirects") {
    Client g{};
    g.set_verify_peer(false);

    g.get(
        "https://localhost:5443/double_redirect",
        [](const Request &r) {
            CHECK(r.url() == "https://localhost:5443/double_redirect");
            CHECK(r.response_code() == 302);
        },
        {}, 1);
}

TEST_CASE("Basic manual POST request") {
    Client g{};
    g.set_verify_peer(false);

    auto r = std::make_shared<Request>(&g, Request::Method::Post, "https://localhost:5443/post");
    r->request("ABCD");
    r->on_complete([](const Request &r) {
        CHECK(r.url() == "https://localhost:5443/post");
        CHECK(r.response_code() == 200);
        CHECK(r.response() == "ABCD");
    });

    g.submit_request(r);
}

TEST_CASE("Basic manual PUT request") {
    Client g{};
    g.set_verify_peer(false);

    auto r = std::make_shared<Request>(&g, Request::Method::Put, "https://localhost:5443/put");
    r->request("ABCD");
    r->on_complete([](const Request &r) {
        CHECK(r.url() == "https://localhost:5443/put");
        CHECK(r.response_code() == 200);
        CHECK(r.response() == "ABCD");
    });

    g.submit_request(r);
}

TEST_CASE("Basic manual HEAD request") {
    Client g{};
    g.set_verify_peer(false);

    auto r = std::make_shared<Request>(&g, Request::Method::Head, "https://localhost:5443/");
    r->on_complete([](const Request &r) {
        CHECK(r.url() == "https://localhost:5443/");
        CHECK(r.response_code() == 200);
        CHECK(r.response().empty());
    });

    g.submit_request(r);
}

TEST_CASE("Basic manual OPTIONS request") {
    Client g{};
    g.set_verify_peer(false);

    auto r = std::make_shared<Request>(&g, Request::Method::Options, "https://localhost:5443/");
    r->on_complete([](const Request &r) {
        CHECK(r.url() == "https://localhost:5443/");
        CHECK(r.response_code() == 200);
        CHECK(r.response_headers()["allow"].find("HEAD") != std::string::npos);
        CHECK(r.response_headers()["allow"].find("OPTIONS") != std::string::npos);
        CHECK(r.response_headers()["allow"].find("GET") != std::string::npos);
    });

    g.submit_request(r);
}

TEST_CASE("Basic manual DELETE request") {
    Client g{};
    g.set_verify_peer(false);

    auto r = std::make_shared<Request>(&g, Request::Method::Delete, "https://localhost:5443/delete");
    r->on_complete([](const Request &r) {
        CHECK(r.url() == "https://localhost:5443/delete");
        CHECK(r.response_code() == 200);
    });

    g.submit_request(r);
}

TEST_CASE("Basic simple POST request") {
    Client g{};
    g.set_verify_peer(false);

    g.post("https://localhost:5443/post", "ABCD", "text/plain", [](const Request &r) {
        CHECK(r.url() == "https://localhost:5443/post");
        CHECK(r.response_code() == 200);
        CHECK(r.response() == "ABCD");
    });
}

TEST_CASE("Basic simple PUT request") {
    Client g{};
    g.set_verify_peer(false);

    g.put("https://localhost:5443/put", "ABCD", "text/plain", [](const Request &r) {
        CHECK(r.url() == "https://localhost:5443/put");
        CHECK(r.response_code() == 200);
        CHECK(r.response() == "ABCD");
    });
}

TEST_CASE("Basic simple HEAD request") {
    Client g{};
    g.set_verify_peer(false);

    g.head("https://localhost:5443/", [](const Request &r) {
        CHECK(r.url() == "https://localhost:5443/");
        CHECK(r.response_code() == 200);
        CHECK(r.response().empty());
    });
}

TEST_CASE("Basic simple OPTIONS request") {
    Client g{};
    g.set_verify_peer(false);

    g.options("https://localhost:5443/", [](const Request &r) {
        CHECK(r.url() == "https://localhost:5443/");
        CHECK(r.response_code() == 200);
        CHECK(r.response_headers()["allow"].find("HEAD") != std::string::npos);
        CHECK(r.response_headers()["allow"].find("OPTIONS") != std::string::npos);
        CHECK(r.response_headers()["allow"].find("GET") != std::string::npos);
    });
}

TEST_CASE("Basic simple DELETE request") {
    Client g{};
    g.set_verify_peer(false);

    g.delete_("https://localhost:5443/delete", [](const Request &r) {
        CHECK(r.url() == "https://localhost:5443/delete");
        CHECK(r.response_code() == 200);
    });
}

TEST_CASE("Basic simple DELETE request with body") {
    Client g{};
    g.set_verify_peer(false);

    g.delete_("https://localhost:5443/delete", "hello", "text/plain", [](const Request &r) {
        CHECK(r.url() == "https://localhost:5443/delete");
        CHECK(r.response_code() == 200);
        CHECK(r.response() == "hello");
    });
}

TEST_CASE("Shutdown") {
    Client g{};

    g.get("https://localhost:5443/", [&g](const Request &r) {
        CHECK(r.url() == "https://localhost:5443/");
        CHECK(r.response_code() != 200);

        g.get("https://localhost:5443/", [](const Request &r) {
            CHECK(r.url() == "https://localhost:5443/");
            CHECK(r.error_code() == CURLE_ABORTED_BY_CALLBACK);
        });
    });
    g.shutdown();
}
