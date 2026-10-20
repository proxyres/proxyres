#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <string>

#include <gtest/gtest.h>

#include "proxyres.h"
#include "http_server.h"

// Long enough for the system resolver to download and evaluate a PAC script
#define RESOLVE_TIMEOUT_MS (30000)

// Config overrides are global so they are cleared before and after each test
class resolver : public ::testing::Test {
   protected:
    void SetUp() override {
        reset_overrides();
    }
    void TearDown() override {
        reset_overrides();
    }

    static void reset_overrides(void) {
        proxy_config_set_auto_config_url_override(NULL);
        proxy_config_set_proxy_override(NULL);
        proxy_config_set_bypass_list_override(NULL);
    }

    // Resolve the proxy list for a url, or an empty string if there is none
    static std::string resolve(const char *url) {
        void *proxy_resolver = proxy_resolver_create();
        if (!proxy_resolver)
            return "";
        EXPECT_TRUE(proxy_resolver_get_proxies_for_url(proxy_resolver, url));
        EXPECT_TRUE(proxy_resolver_wait(proxy_resolver, RESOLVE_TIMEOUT_MS));
        const char *list = proxy_resolver_get_list(proxy_resolver);
        std::string result = list ? list : "";
        proxy_resolver_delete(&proxy_resolver);
        return result;
    }
};

// Serve a PAC script for every request
static std::string pac_response(const std::string &script) {
    return http_response("200 OK", "Content-Length: " + std::to_string(script.size()) + "\r\n", script);
}

// Subset of pac.js with the proxy types that every system resolver supports
static const char *pac_script = R"(
function FindProxyForURL(url, host) {
  if (host == "simple.com") {
    return "PROXY no-such-proxy:80";
  } else if (host == "multi-legacy.com") {
    return "PROXY some-such-proxy:443;PROXY any-such-proxy:41";
  } else if (host == "multi-legacy-direct.com") {
    return "PROXY some-such-proxy:443;PROXY any-such-proxy:41;DIRECT";
  }
  return "DIRECT";
})";

TEST_F(resolver, global_init) {
    EXPECT_TRUE(proxyres_global_init());
    EXPECT_TRUE(proxy_config_global_init());
    EXPECT_TRUE(proxy_resolver_global_init());
    EXPECT_TRUE(proxyres_global_cleanup());
    EXPECT_TRUE(proxy_config_global_cleanup());
    EXPECT_TRUE(proxy_resolver_global_cleanup());
}

TEST_F(resolver, manual_proxy) {
    proxy_config_set_auto_config_url_override("http://127.0.0.1:8080/pac.js");
    char *auto_config_url = proxy_config_get_auto_config_url();
    EXPECT_STREQ(auto_config_url, "http://127.0.0.1:8080/pac.js");
    free(auto_config_url);
    proxy_config_set_auto_config_url_override(NULL);

    proxy_config_set_proxy_override("my.proxy:8080");
    proxy_config_set_bypass_list_override("*.example.com");
    EXPECT_FALSE(proxy_config_get_auto_discover());

    void *proxy_resolver = proxy_resolver_create();
    ASSERT_NE(proxy_resolver, nullptr);
    EXPECT_EQ(proxy_resolver_get_list(proxy_resolver), nullptr);
    EXPECT_TRUE(proxy_resolver_get_proxies_for_url(proxy_resolver, "https://example.org/"));
    EXPECT_TRUE(proxy_resolver_wait(proxy_resolver, -1));
    EXPECT_STREQ(proxy_resolver_get_list(proxy_resolver), "http://my.proxy:8080");
    char *proxy = proxy_resolver_get_next_proxy(proxy_resolver);
    EXPECT_STREQ(proxy, "http://my.proxy:8080");
    free(proxy);
    EXPECT_EQ(proxy_resolver_get_next_proxy(proxy_resolver), nullptr);
    EXPECT_EQ(proxy_resolver_get_error(proxy_resolver), 0);

    // Bypassed url uses a direct connection
    EXPECT_TRUE(proxy_resolver_get_proxies_for_url(proxy_resolver, "https://www.example.com/"));
    EXPECT_TRUE(proxy_resolver_wait(proxy_resolver, 0));
    EXPECT_STREQ(proxy_resolver_get_list(proxy_resolver), "direct://");
    // Cancelling a completed resolution succeeds
    EXPECT_TRUE(proxy_resolver_cancel(proxy_resolver));

    EXPECT_TRUE(proxy_resolver_delete(&proxy_resolver));
    EXPECT_EQ(proxy_resolver, nullptr);
    EXPECT_FALSE(proxy_resolver_delete(&proxy_resolver));
}

TEST_F(resolver, manual_proxy_localhost) {
    proxy_config_set_proxy_override("my.proxy:8080");
    EXPECT_EQ(resolve("http://example.com/"), "http://my.proxy:8080");
    // Localhost is bypassed by default
    EXPECT_EQ(resolve("http://localhost/"), "direct://");
}

TEST_F(resolver, manual_proxy_with_scheme) {
    // Proxy with a scheme keeps its scheme
    proxy_config_set_proxy_override("https://my.proxy/");
    EXPECT_EQ(resolve("http://example.com/"), "https://my.proxy:443");
}

TEST_F(resolver, manual_proxy_bypass) {
    proxy_config_set_proxy_override("my.proxy:8080");
    proxy_config_set_bypass_list_override("*.example.com,10.0.0.0/8");
    EXPECT_EQ(resolve("http://www.example.com/"), "direct://");
    EXPECT_EQ(resolve("http://10.1.2.3/"), "direct://");
    EXPECT_EQ(resolve("http://example.org/"), "http://my.proxy:8080");
}

TEST_F(resolver, null_instances) {
    EXPECT_FALSE(proxy_resolver_get_proxies_for_url(NULL, NULL));
    EXPECT_EQ(proxy_resolver_get_list(NULL), nullptr);
    EXPECT_EQ(proxy_resolver_get_next_proxy(NULL), nullptr);
    EXPECT_EQ(proxy_resolver_get_error(NULL), -1);
    EXPECT_FALSE(proxy_resolver_wait(NULL, 0));
    EXPECT_FALSE(proxy_resolver_cancel(NULL));
    EXPECT_FALSE(proxy_resolver_delete(NULL));
}

TEST_F(resolver, wait_without_request) {
    void *proxy_resolver = proxy_resolver_create();
    ASSERT_NE(proxy_resolver, nullptr);
    EXPECT_FALSE(proxy_resolver_wait(proxy_resolver, 0));
    EXPECT_EQ(proxy_resolver_get_list(proxy_resolver), nullptr);
    EXPECT_EQ(proxy_resolver_get_next_proxy(proxy_resolver), nullptr);
    EXPECT_EQ(proxy_resolver_get_error(proxy_resolver), 0);
    proxy_resolver_delete(&proxy_resolver);
}

TEST_F(resolver, direct_without_configuration) {
    char *auto_config_url = proxy_config_get_auto_config_url();
    char *proxy = proxy_config_get_proxy("http");
    bool is_configured = auto_config_url || proxy || proxy_config_get_auto_discover();
    free(auto_config_url);
    free(proxy);
    if (is_configured)
        GTEST_SKIP() << "System proxy configuration is set";

    void *proxy_resolver = proxy_resolver_create();
    ASSERT_NE(proxy_resolver, nullptr);
    EXPECT_TRUE(proxy_resolver_get_proxies_for_url(proxy_resolver, "http://example.com/"));
    EXPECT_TRUE(proxy_resolver_wait(proxy_resolver, RESOLVE_TIMEOUT_MS));
    EXPECT_STREQ(proxy_resolver_get_list(proxy_resolver), "direct://");
    proxy_resolver_delete(&proxy_resolver);
}

TEST_F(resolver, pac_url) {
    const std::string script = R"(
function FindProxyForURL(url, host) {
  return "SOCKS socks.example.com:1080; PROXY proxy.example.com:8080; DIRECT";
})";
    http_server server(http_response("200 OK", "Content-Length: " + std::to_string(script.size()) + "\r\n", script));
    proxy_config_set_auto_config_url_override(server.url().c_str());

    void *proxy_resolver = proxy_resolver_create();
    ASSERT_NE(proxy_resolver, nullptr);
    EXPECT_TRUE(proxy_resolver_get_proxies_for_url(proxy_resolver, "http://example.com/"));
    EXPECT_TRUE(proxy_resolver_wait(proxy_resolver, RESOLVE_TIMEOUT_MS));
    EXPECT_EQ(proxy_resolver_get_error(proxy_resolver), 0);
    EXPECT_STREQ(proxy_resolver_get_list(proxy_resolver),
                 "socks://socks.example.com:1080,http://proxy.example.com:8080,direct://");
    proxy_resolver_delete(&proxy_resolver);
}

TEST_F(resolver, pac_url_refused) {
    proxy_config_set_auto_config_url_override(http_refused_url("/pac.js").c_str());

    void *proxy_resolver = proxy_resolver_create();
    ASSERT_NE(proxy_resolver, nullptr);
    EXPECT_TRUE(proxy_resolver_get_proxies_for_url(proxy_resolver, "http://example.com/"));
    EXPECT_TRUE(proxy_resolver_wait(proxy_resolver, RESOLVE_TIMEOUT_MS));
    EXPECT_NE(proxy_resolver_get_error(proxy_resolver), 0);
    EXPECT_EQ(proxy_resolver_get_list(proxy_resolver), nullptr);
    proxy_resolver_delete(&proxy_resolver);
}

TEST_F(resolver, pac_url_script_failure) {
    http_server server(pac_response("function FindProxyForURL(url, host) { throw 'x'; }"));
    proxy_config_set_auto_config_url_override(server.url("/pac.js").c_str());

    void *proxy_resolver = proxy_resolver_create();
    ASSERT_NE(proxy_resolver, nullptr);
    EXPECT_TRUE(proxy_resolver_get_proxies_for_url(proxy_resolver, "http://example.com/"));
    EXPECT_TRUE(proxy_resolver_wait(proxy_resolver, RESOLVE_TIMEOUT_MS));
    EXPECT_NE(proxy_resolver_get_error(proxy_resolver), 0);
    EXPECT_EQ(proxy_resolver_get_list(proxy_resolver), nullptr);
    proxy_resolver_delete(&proxy_resolver);
}

TEST_F(resolver, pac_url_concurrent) {
    http_server server(pac_response(pac_script));
    proxy_config_set_auto_config_url_override(server.url("/pac.js").c_str());

    // Resolve on several threads at once while none of them has cached the script yet
    void *proxy_resolvers[8] = {};
    for (auto &proxy_resolver : proxy_resolvers) {
        proxy_resolver = proxy_resolver_create();
        ASSERT_NE(proxy_resolver, nullptr);
    }
    for (auto &proxy_resolver : proxy_resolvers)
        EXPECT_TRUE(proxy_resolver_get_proxies_for_url(proxy_resolver, "http://simple.com/"));
    for (auto &proxy_resolver : proxy_resolvers) {
        EXPECT_TRUE(proxy_resolver_wait(proxy_resolver, RESOLVE_TIMEOUT_MS));
        EXPECT_STREQ(proxy_resolver_get_list(proxy_resolver), "http://no-such-proxy:80");
        proxy_resolver_delete(&proxy_resolver);
    }
}

TEST_F(resolver, next_proxy) {
    http_server server(pac_response(pac_script));
    proxy_config_set_auto_config_url_override(server.url("/pac.js").c_str());

    void *proxy_resolver = proxy_resolver_create();
    ASSERT_NE(proxy_resolver, nullptr);
    EXPECT_TRUE(proxy_resolver_get_proxies_for_url(proxy_resolver, "https://multi-legacy-direct.com/"));
    EXPECT_EQ(proxy_resolver_get_next_proxy(proxy_resolver), nullptr);
    EXPECT_TRUE(proxy_resolver_wait(proxy_resolver, RESOLVE_TIMEOUT_MS));
    for (const char *expected : {"http://some-such-proxy:443", "http://any-such-proxy:41", "direct://"}) {
        char *proxy = proxy_resolver_get_next_proxy(proxy_resolver);
        EXPECT_STREQ(proxy, expected);
        free(proxy);
    }
    EXPECT_EQ(proxy_resolver_get_next_proxy(proxy_resolver), nullptr);
    proxy_resolver_delete(&proxy_resolver);
}

struct resolver_pac_url_param {
    const char *url;
    const char *expected;

    friend std::ostream &operator<<(std::ostream &os, const resolver_pac_url_param &param) {
        return os << "url: " << param.url;
    }
};

constexpr resolver_pac_url_param resolver_pac_url_tests[] = {
    {"https://google.com/", "direct://"},
    {"http://simple.com/", "http://no-such-proxy:80"},
    {"https://simple.com/", "http://no-such-proxy:80"},
    {"https://multi-legacy.com/", "http://some-such-proxy:443,http://any-such-proxy:41"},
};

class resolver_pac_url : public resolver, public ::testing::WithParamInterface<resolver_pac_url_param> {};

INSTANTIATE_TEST_SUITE_P(resolver, resolver_pac_url, testing::ValuesIn(resolver_pac_url_tests));

TEST_P(resolver_pac_url, get_proxies_for_url) {
    const auto &param = GetParam();
    http_server server(pac_response(pac_script));
    proxy_config_set_auto_config_url_override(server.url("/pac.js").c_str());
    EXPECT_EQ(resolve(param.url), param.expected);
}
