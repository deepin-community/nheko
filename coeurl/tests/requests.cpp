#include <thread>

#include "coeurl/client.hpp"
#include "coeurl/request.hpp"

#if __has_include(<doctest.h>)
#include <doctest.h>
#else
#include <doctest/doctest.h>
#endif

using namespace coeurl;

TEST_CASE("Basic request") {
    Client g{};

    g.get("http://localhost:5000/", [](const Request &r) {
        CHECK(r.url() == "http://localhost:5000/");
        CHECK(r.response_code() == 200);
        CHECK(r.response() == "OK");
        CHECK(r.response_headers()["content-type"] == "text/plain; charset=utf-8");
    });
}

TEST_CASE("Basic manual request") {
    Client g{};

    auto r = std::make_shared<Request>(&g, Request::Method::Get, "http://localhost:5000/");
    r->on_complete([](const Request &r) {
        CHECK(r.url() == "http://localhost:5000/");
        CHECK(r.response_code() == 200);
        CHECK(r.response() == "OK");
        CHECK(r.response_headers()["content-type"] == "text/plain; charset=utf-8");
    });

    g.submit_request(r);
}

TEST_CASE("Basic redirect") {
    Client g{};

    g.get(
        "http://localhost:5000/redirect",
        [](const Request &r) {
            CHECK(r.url() == "http://localhost:5000/redirect");
            CHECK(r.response_code() == 200);
            CHECK(r.response() == "OK");
        },
        {}, 1);
}

TEST_CASE("No redirect") {
    Client g{};

    g.get(
        "http://localhost:5000/redirect",
        [](const Request &r) {
            CHECK(r.url() == "http://localhost:5000/redirect");
            CHECK(r.response_code() == 302);
        },
        {}, 0);
}

TEST_CASE("Max redirects") {
    Client g{};

    g.get(
        "http://localhost:5000/double_redirect",
        [](const Request &r) {
            CHECK(r.url() == "http://localhost:5000/double_redirect");
            CHECK(r.response_code() == 302);
        },
        {}, 1);
}

TEST_CASE("Basic manual POST request") {
    Client g{};

    auto r = std::make_shared<Request>(&g, Request::Method::Post, "http://localhost:5000/post");
    r->request("ABCD");
    r->on_complete([](const Request &r) {
        CHECK(r.url() == "http://localhost:5000/post");
        CHECK(r.response_code() == 200);
        CHECK(r.response() == "ABCD");
    });

    g.submit_request(r);
}

TEST_CASE("Basic manual PUT request") {
    Client g{};

    auto r = std::make_shared<Request>(&g, Request::Method::Put, "http://localhost:5000/put");
    r->request("ABCD");
    r->on_complete([](const Request &r) {
        CHECK(r.url() == "http://localhost:5000/put");
        CHECK(r.response_code() == 200);
        CHECK(r.response() == "ABCD");
    });

    g.submit_request(r);
}

TEST_CASE("Basic manual HEAD request") {
    Client g{};

    auto r = std::make_shared<Request>(&g, Request::Method::Head, "http://localhost:5000/");
    r->on_complete([](const Request &r) {
        CHECK(r.url() == "http://localhost:5000/");
        CHECK(r.response_code() == 200);
        CHECK(r.response().empty());
    });

    g.submit_request(r);
}

TEST_CASE("Basic manual OPTIONS request") {
    Client g{};

    auto r = std::make_shared<Request>(&g, Request::Method::Options, "http://localhost:5000/");
    r->on_complete([](const Request &r) {
        CHECK(r.url() == "http://localhost:5000/");
        CHECK(r.response_code() == 200);
        CHECK(r.response_headers()["allow"].find("HEAD") != std::string::npos);
        CHECK(r.response_headers()["allow"].find("OPTIONS") != std::string::npos);
        CHECK(r.response_headers()["allow"].find("GET") != std::string::npos);
    });

    g.submit_request(r);
}

TEST_CASE("Basic manual DELETE request") {
    Client g{};

    auto r = std::make_shared<Request>(&g, Request::Method::Delete, "http://localhost:5000/delete");
    r->on_complete([](const Request &r) {
        CHECK(r.url() == "http://localhost:5000/delete");
        CHECK(r.response_code() == 200);
    });

    g.submit_request(r);
}

TEST_CASE("Basic simple POST request") {
    Client g{};

    g.post("http://localhost:5000/post", "ABCD", "text/plain", [](const Request &r) {
        CHECK(r.url() == "http://localhost:5000/post");
        CHECK(r.response_code() == 200);
        CHECK(r.response() == "ABCD");
    });
}

TEST_CASE("Basic simple PUT request") {
    Client g{};

    g.put("http://localhost:5000/put", "ABCD", "text/plain", [](const Request &r) {
        CHECK(r.url() == "http://localhost:5000/put");
        CHECK(r.response_code() == 200);
        CHECK(r.response() == "ABCD");
    });
}

TEST_CASE("Basic simple HEAD request") {
    Client g{};

    g.head("http://localhost:5000/", [](const Request &r) {
        CHECK(r.url() == "http://localhost:5000/");
        CHECK(r.response_code() == 200);
        CHECK(r.response().empty());
    });
}

TEST_CASE("Basic simple OPTIONS request") {
    Client g{};

    g.options("http://localhost:5000/", [](const Request &r) {
        CHECK(r.url() == "http://localhost:5000/");
        CHECK(r.response_code() == 200);
        CHECK(r.response_headers()["allow"].find("HEAD") != std::string::npos);
        CHECK(r.response_headers()["allow"].find("OPTIONS") != std::string::npos);
        CHECK(r.response_headers()["allow"].find("GET") != std::string::npos);
    });
}

TEST_CASE("Basic simple DELETE request") {
    Client g{};

    g.delete_("http://localhost:5000/delete", [](const Request &r) {
        CHECK(r.url() == "http://localhost:5000/delete");
        CHECK(r.response_code() == 200);
    });
}

TEST_CASE("Basic simple DELETE request with body") {
    Client g{};

    g.delete_("http://localhost:5000/delete", "hello", "text/plain", [](const Request &r) {
        CHECK(r.url() == "http://localhost:5000/delete");
        CHECK(r.response_code() == 200);
        CHECK(r.response() == "hello");
    });
}

TEST_CASE("Shutdown") {
    Client g{};

    g.get("http://localhost:5000/", [&g](const Request &r) {
        CHECK(r.url() == "http://localhost:5000/");
        CHECK(r.response_code() != 200);

        g.get("http://localhost:5000/", [](const Request &r) {
            CHECK(r.url() == "http://localhost:5000/");
            CHECK(r.error_code() == CURLE_ABORTED_BY_CALLBACK);
        });
    });
    g.shutdown();
}
