#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#  include <objbase.h>
#endif

#include <gtest/gtest.h>

#include "execute.h"
#include "net_util.h"
#include "util.h"

struct execute_param {
    const char *url;
    const char *expected;

    friend std::ostream &operator<<(std::ostream &os, const execute_param &param) {
        return os << "url: " << param.url;
    }
};

static const char *script = R"(
function FindProxyForURL(url, host) {
  if (host == "myip") {
    return "PROXY " + myIpAddress() + ":80";
  }
  if (host == "myipex") {
    var addresses = myIpAddressEx().split(';');
    var proxies = "";
    for (var i = 0; i < addresses.length; i++) {
      proxies += "PROXY " + addresses[i] + ":80";
      if (i != addresses.length - 1) {
        proxies += ";";
      }
    }
    return proxies;
  }
  if (host == "localhost") {
    return "PROXY " + dnsResolve(host) + ":80";
  }
  if (host == "::1") {
    return "PROXY " + dnsResolveEx(host) + ":80";
  }
  if (host == "127.0.0.1") {
    return "PROXY localhost:30";
  }
  if (isPlainHostName(host)) {
    return "HTTP plain";
  }
  if (host == "simple.com") {
    return "PROXY no-such-proxy:80";
  }
  if (shExpMatch(url, '*microsoft.com/*')) {
    return "PROXY microsoft.com:80";
  }
  return "DIRECT";
})";

constexpr execute_param execute_tests[] = {{"your-pc", "HTTP plain"},
                                           {"127.0.0.1", "PROXY localhost:30"},
                                           {"localhost", "PROXY 127.0.0.1:80"},
                                           {"::1", "PROXY ::1:80"},
                                           {"myip", NULL},
                                           {"myipex", NULL},
                                           {"http://127.0.0.1/", "PROXY localhost:30"},
                                           {"http://simple.com/", "PROXY no-such-proxy:80"},
                                           {"http://example2.com/", "DIRECT"},
                                           {"http://microsoft.com/test", "PROXY microsoft.com:80"},
                                           {"file:///c:/test", NULL},
                                           {"file:////home/test", NULL}};

class execute : public ::testing::TestWithParam<execute_param> {};

INSTANTIATE_TEST_SUITE_P(execute, execute, testing::ValuesIn(execute_tests));

// Construct expected output for myip test case
static char *expected_my_ip(void) {
    char *address = my_ip_address();
    if (!address)
        return NULL;

    size_t max_expected = strlen(address) + 16;
    char *expected = (char *)calloc(max_expected, sizeof(char));
    if (!expected)
        return NULL;

    snprintf(expected, max_expected, "PROXY %s:80", address);
    free(address);
    return expected;
}

// Construct expected output for myipex test case
static char *expected_my_ip_ex(void) {
    char *addresses = my_ip_address_ex();
    if (!addresses)
        return NULL;

    int32_t address_count = str_count_chr(addresses, ';') + 1;
    size_t max_expected = (address_count + 16) * HOST_MAX;
    char *expected = (char *)calloc(max_expected, sizeof(char));
    if (!expected)
        return NULL;

    // Enumerate each address and append expected proxy to string
    const char *addressesp = addresses;
    while (addressesp) {
        char *address = str_sep_dup(&addressesp, ";");
        size_t expected_len = strlen(expected);
        if (expected_len) {
            strncat(expected, ";", max_expected - expected_len);
            expected_len++;
        }
        snprintf(expected + expected_len, max_expected - expected_len, "PROXY %s:80", address);
        free(address);
    }
    return expected;
}

TEST_P(execute, get_proxies_for_url) {
    const auto &param = GetParam();
    void *proxy_execute = proxy_execute_create();
    EXPECT_NE(proxy_execute, nullptr);
    if (proxy_execute) {
        EXPECT_TRUE(proxy_execute_get_proxies_for_url(proxy_execute, script, param.url));
        EXPECT_EQ(proxy_execute_get_error(proxy_execute), 0);
        const char *list = proxy_execute_get_list(proxy_execute);
        EXPECT_NE(list, nullptr);
        if (list) {
            if (!param.expected) {
                // Test expected result from myIpAddress() and myIpAddressEx()
                if (strcmp(param.url, "myip") == 0) {
                    char *expected = expected_my_ip();
                    EXPECT_NE(expected, nullptr);
                    if (expected)
                        EXPECT_STREQ(list, expected);
                    free(expected);
                } else if (strcmp(param.url, "myipex") == 0) {
                    char *expected = expected_my_ip_ex();
                    EXPECT_NE(expected, nullptr);
                    if (expected)
                        EXPECT_STREQ(list, expected);
                    free(expected);
                }
            } else {
                EXPECT_STREQ(list, param.expected);
            }
        }
        proxy_execute_delete(&proxy_execute);
    }
}

TEST(execute, null_instance) {
    void *proxy_execute = NULL;
    EXPECT_FALSE(proxy_execute_get_proxies_for_url(NULL, "", "http://a/"));
    EXPECT_EQ(proxy_execute_get_list(NULL), nullptr);
    EXPECT_EQ(proxy_execute_get_error(NULL), -1);
    EXPECT_FALSE(proxy_execute_delete(NULL));
    EXPECT_FALSE(proxy_execute_delete(&proxy_execute));
}

constexpr const char *execute_error_scripts[] = {
    // Syntax error
    "function FindProxyForURL(url, host) {",
    // Missing FindProxyForURL
    "var a = 1;",
    // Thrown Error
    "function FindProxyForURL(url, host) { throw new Error('boom'); }",
    // Thrown value that is not an Error
    "function FindProxyForURL(url, host) { throw 42; }",
    // Undefined function
    "function FindProxyForURL(url, host) { return notDefined(host); }",
    // Result that is not a string
    "function FindProxyForURL(url, host) { return undefined; }",
};

class execute_error : public ::testing::TestWithParam<const char *> {};

INSTANTIATE_TEST_SUITE_P(execute, execute_error, testing::ValuesIn(execute_error_scripts));

TEST_P(execute_error, get_proxies_for_url) {
    void *proxy_execute = proxy_execute_create();
    ASSERT_NE(proxy_execute, nullptr);
    EXPECT_FALSE(proxy_execute_get_proxies_for_url(proxy_execute, GetParam(), "http://a/"));
    EXPECT_NE(proxy_execute_get_error(proxy_execute), 0);
    proxy_execute_delete(&proxy_execute);
}

TEST(execute, concurrent) {
    // Executions on different threads keep their own native functions and results
    std::vector<std::thread> threads;
    for (int32_t i = 0; i < 8; i++) {
        threads.emplace_back([i] {
            const std::string proxy = "PROXY t" + std::to_string(i) + "-";
            const std::string script = "function FindProxyForURL(url, host) { return \"" + proxy +
                "\" + dnsResolve(\"127.0.0.1\") + \":80\"; }";
            const std::string expected = proxy + "127.0.0.1:80";
            for (int32_t j = 0; j < 10; j++) {
                void *proxy_execute = proxy_execute_create();
                ASSERT_NE(proxy_execute, nullptr);
                EXPECT_TRUE(proxy_execute_get_proxies_for_url(proxy_execute, script.c_str(), "http://a/"));
                EXPECT_STREQ(proxy_execute_get_list(proxy_execute), expected.c_str());
                proxy_execute_delete(&proxy_execute);
            }
        });
    }
    for (auto &thread : threads)
        thread.join();
}

#ifdef _WIN32
TEST(execute, keeps_caller_com_initialization) {
    std::thread thread([] {
        ASSERT_TRUE(SUCCEEDED(CoInitializeEx(NULL, COINIT_MULTITHREADED)));
        void *proxy_execute = proxy_execute_create();
        EXPECT_NE(proxy_execute, nullptr);
        if (proxy_execute) {
            EXPECT_TRUE(proxy_execute_get_proxies_for_url(
                proxy_execute, "function FindProxyForURL(url, host) { return 'DIRECT'; }", "http://a/"));
            EXPECT_STREQ(proxy_execute_get_list(proxy_execute), "DIRECT");
            proxy_execute_delete(&proxy_execute);
        }
        // The caller's multithreaded apartment is still initialized
        EXPECT_EQ(CoInitializeEx(NULL, COINIT_MULTITHREADED), S_FALSE);
        CoUninitialize();
        CoUninitialize();
    });
    thread.join();
}
#endif
