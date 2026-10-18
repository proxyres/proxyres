#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <string>

#include <gtest/gtest.h>

#include "fetch.h"
#include "http_server.h"

TEST(fetch, get_local) {
    const std::string script = "function FindProxyForURL(url, host) { return 'DIRECT'; }";
    http_server server(http_response("200 OK", "Content-Length: " + std::to_string(script.size()) + "\r\n", script));
    int32_t error = 0;
    char *body = fetch_get(server.url().c_str(), &error);
    EXPECT_EQ(error, 0);
    ASSERT_NE(body, nullptr);
    EXPECT_STREQ(body, script.c_str());
    free(body);
}

TEST(fetch, missing_host) {
    char *body = fetch_get("http:///wpad.dat", NULL);
    EXPECT_EQ(body, nullptr);
    free(body);
}

#ifndef HAVE_CURL
TEST(fetch, chunked_not_supported) {
    http_server server(http_response("200 OK", "Transfer-Encoding: chunked\r\n", "4\r\nfunc\r\n0\r\n\r\n"));
    int32_t error = 0;
    char *body = fetch_get(server.url().c_str(), &error);
    EXPECT_EQ(body, nullptr);
    EXPECT_NE(error, 0);
    free(body);
}

TEST(fetch, headers_too_large) {
    const std::string headers = "X-Padding: " + std::string(2048, 'x') + "\r\nContent-Length: 8\r\n";
    http_server server(http_response("200 OK", headers, "function"));
    int32_t error = 0;
    char *body = fetch_get(server.url().c_str(), &error);
    EXPECT_EQ(body, nullptr);
    EXPECT_NE(error, 0);
    free(body);
}
#endif

TEST(fetch, get) {
    int32_t error = 0;
#ifdef HAVE_CURL
    const char *url = "https://google.com/";
    const char *expected_string = "world's information";
#else
    const char *url = "http://google.com/";
    const char *expected_string = "document has moved";
#endif
    char *body = fetch_get(url, &error);
    EXPECT_EQ(error, 0);
    EXPECT_NE(body, nullptr);
    if (body) {
        EXPECT_TRUE(strstr(body, expected_string) != nullptr);
        free(body);
    }
}
