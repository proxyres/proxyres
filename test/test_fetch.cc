#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <string>

#include <gtest/gtest.h>

#include "fetch.h"
#include "http_server.h"
#include "util.h"

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

TEST(fetch, too_large) {
    const std::string script(SCRIPT_MAX + 1, 'x');
    http_server server(http_response("200 OK", "Content-Length: " + std::to_string(script.size()) + "\r\n", script));
    int32_t error = 0;
    char *body = fetch_get(server.url().c_str(), &error);
    EXPECT_EQ(body, nullptr);
    EXPECT_NE(error, 0);
    free(body);
}

TEST(fetch, connection_refused) {
    int32_t error = 0;
    char *body = fetch_get(http_refused_url("/wpad.dat").c_str(), &error);
    EXPECT_EQ(body, nullptr);
    EXPECT_NE(error, 0);
    free(body);
}

TEST(fetch, missing_host) {
    int32_t error = 0;
    char *body = fetch_get("http:///wpad.dat", &error);
    EXPECT_EQ(body, nullptr);
    EXPECT_NE(error, 0);
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

TEST(fetch, get_large_body) {
    const std::string script(100 * 1024, 'x');
    http_server server(http_response("200 OK", "content-length: " + std::to_string(script.size()) + "\r\n", script));
    int32_t error = 0;
    char *body = fetch_get(server.url().c_str(), &error);
    EXPECT_EQ(error, 0);
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(strlen(body), script.size());
    free(body);
}

TEST(fetch, body_longer_than_content_length) {
    std::string script;
    for (int32_t i = 0; i < 100; i++)
        script += "function";
    http_server server(http_response("200 OK", "Content-Length: 4\r\n", script));
    int32_t error = 0;
    char *body = fetch_get(server.url().c_str(), &error);
    EXPECT_EQ(error, 0);
    EXPECT_STREQ(body, "func");
    free(body);
}

TEST(fetch, https_not_supported) {
    int32_t error = 0;
    char *body = fetch_get("https://127.0.0.1/wpad.dat", &error);
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
