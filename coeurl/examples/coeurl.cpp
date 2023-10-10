#include <thread>

#include "coeurl/client.hpp"
#include "coeurl/request.hpp"

#include "spdlog/sinks/stdout_color_sinks.h"

int main(int, char **) {
    auto logger = spdlog::stdout_color_mt("coeurl");
    logger->set_level(spdlog::level::trace);
    coeurl::Client::set_logger(logger);

    coeurl::Client g{};

    std::thread t([&g, &logger]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        for (int i = 0; i < 10; i++)
            g.get("http://localhost:5000/", [](const coeurl::Request &) {});

        std::this_thread::sleep_for(std::chrono::seconds(1));
        for (int i = 0; i < 10; i++)
            g.get("http://localhost:5000/", [&logger](const coeurl::Request &r) {
                for (const auto &[k, v] : r.response_headers())
                    logger->info("Header: {}: {}", k, v);
            });

        auto r = std::make_shared<coeurl::Request>(&g, coeurl::Request::Method::Post, "http://localhost:5000/post");
        r->request("ABCD");
        r->on_complete([](const coeurl::Request &) {});
        g.submit_request(r);
    });

    t.join();

    return 0;
}
