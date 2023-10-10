coeurl
======

[![Pipeline Status](https://nheko.im/nheko-reborn/coeurl/badges/master/pipeline.svg?ignore_skipped=true)](https://nheko.im/nheko-reborn/coeurl/-/pipelines)
[![Coverage](https://nheko.im/nheko-reborn/coeurl/badges/master/coverage.svg?ignore_skipped=true)](https://nheko.im/nheko-reborn/coeurl/-/pipelines/latest)
[![Documentation](https://img.shields.io/badge/documentation-doxygen-informational)](https://nheko-reborn.pages.nheko.im/coeurl/index.html)
[![#nheko:nheko.im](https://img.shields.io/matrix/nheko-reborn:matrix.org.svg?label=%23nheko:nheko.im)](https://matrix.to/#/#nheko:nheko.im)

Simple library to do http requests asynchronously via CURL in C++. (Eventually
as coroutines, once all the compilers I need to support support them.)

This is based on the [CURL-libevent
example](https://curl.se/libcurl/c/hiperfifo.html).

You can do a get request with 3 simple steps:

1. Initialize the library:
```cpp
coeurl::Client g{};
```
2. Do a request
```cpp
g.get("http://localhost:5000/", [](const coeurl::Request &res) {
    std::cout << res.response() << std::endl;
});
```

If you need more flexibility, you can initialize a `coeurl::Request` manually
and set all the fields required. Currently only a few methods are exposed, but
we may decide to add more in the future or expose the easy handle for direct
manipulation.

Dependencies
------------

- CURL (duh!)
- libevent
- spdlog
- for tests: doctest

Building
--------

Usually meson should do all you need. For example you can build it like this in
a subdirectory called `buildir/`:

```sh
meson setup builddir
meson compile -C builddir
```

There is also a cmake file, but this does not properly support installation. It
is only useful for using this project via ExternalProject.

Limitations
-----------

The event loop can only run on one thread at a time! If you need to parallelize
your request, use multiple clients or dispatch the responses into a threadpool
manually. In most cases the one thread should be enough for the simpler
workloads though and this way you can benefit from connection pooling and the
HTTP/2 multiplexing. The tread is created internally and you currently have no
control over it. You can wait for it to exit using `close()` or the `~Client`
destructor. If you block this thread, no other requests will get processed.

This library also only exposes the simple way to do things currently. This is
all I need at the moment.

Interesting bits
----------------

- You can enable logging or customize the logging by setting a logger with
		`coeurl::Client::set_logger`. Do this before you initialize any client!
- The Request can be constructed builder style, then submitted and then you can
		query the Request once it has completed for the headers, status code, etc.
- Don't modify the Request while it is in flight or call any members on it until
		it is completed, after you submitted it.

