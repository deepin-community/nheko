CHANGELOG
=========

[0.3.0] - 2023-01-10
--------------------

- Support altsvc to properly detect http/3 across runs. You need to specify a
    path where the cache will be stored to enable this.
- Error out when the project is built using CMake, since CMake is only uspported
    for FetchContent purposes.

[0.2.1] - 2022-07-21
--------------------

- Limit concurrent connections to 64 and per host to 8 by default. You can
    change that using 2 new functions.
- Use major.minor as the soname.

[0.2.0] - 2022-03-06
--------------------

- Fix potential hang when the client is shutdown and a request is scheduled at
    the same time.

[0.1.1] - 2021-12-20
--------------------

- Add wrapper function to convert error codes to strings.

[0.1.0] - 2021-11-14
--------------------

- Initial release.
