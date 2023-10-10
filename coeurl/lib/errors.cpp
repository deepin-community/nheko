#include <coeurl/errors.hpp>

const char* coeurl::to_string(CURLcode c) {
    return curl_easy_strerror(c);
}

//const char* coeurl::to_string(CURLUcode c) {
//    return curl_url_strerror(c);
//}

const char* coeurl::to_string(CURLMcode c) {
    return curl_multi_strerror(c);
}

const char* coeurl::to_string(CURLSHcode c) {
    return curl_share_strerror(c);
}
