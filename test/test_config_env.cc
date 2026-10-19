#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <string>
#include <vector>

#include <gtest/gtest.h>

// Internal config headers are only declared for C
extern "C" {
#include "config_i.h"
#include "config_env.h"
}

static const char *proxy_env_var_names[] = {"http_proxy", "HTTP_PROXY",  "https_proxy", "HTTPS_PROXY", "ftp_proxy",
                                            "FTP_PROXY",  "socks_proxy", "SOCKS_PROXY", "no_proxy",    "NO_PROXY"};

class config_env : public ::testing::Test {
   protected:
    void SetUp() override {
        // Clear the proxy variables of the environment running the tests
        for (const char *name : proxy_env_var_names) {
            const char *value = getenv(name);
            saved_vars.push_back({name, value != nullptr, value ? value : ""});
            unsetenv(name);
        }
    }
    void TearDown() override {
        for (const auto &var : saved_vars) {
            if (var.is_set)
                setenv(var.name, var.value.c_str(), 1);
            else
                unsetenv(var.name);
        }
    }

    struct saved_var {
        const char *name;
        bool is_set;
        std::string value;
    };
    std::vector<saved_var> saved_vars;
};

TEST_F(config_env, proxy) {
    setenv("http_proxy", "my.proxy3.com:8003", 1);
    setenv("HTTPS_PROXY", "my.proxy4.com:8004", 1);
    setenv("ftp_proxy", "", 1);
    setenv("FTP_PROXY", "my.proxy5.com:8005", 1);

    char *proxy = proxy_config_env_get_proxy("http");
    EXPECT_STREQ(proxy, "my.proxy3.com:8003");
    free(proxy);
    proxy = proxy_config_env_get_proxy("https");
    EXPECT_STREQ(proxy, "my.proxy4.com:8004");
    free(proxy);
    // Empty lowercase variable falls back to uppercase variable
    proxy = proxy_config_env_get_proxy("ftp");
    EXPECT_STREQ(proxy, "my.proxy5.com:8005");
    free(proxy);
    proxy = proxy_config_env_get_proxy("socks");
    EXPECT_EQ(proxy, nullptr);
    free(proxy);
}

TEST_F(config_env, http_proxy_uppercase_ignored) {
    setenv("HTTP_PROXY", "my.proxy3.com:8003", 1);

    char *proxy = proxy_config_env_get_proxy("http");
    EXPECT_EQ(proxy, nullptr);
    free(proxy);
}

TEST_F(config_env, bypass_list) {
    setenv("NO_PROXY", "localhost,127.0.0.0/8,", 1);

    char *bypass_list = proxy_config_env_get_bypass_list();
    EXPECT_STREQ(bypass_list, "localhost,127.0.0.0/8");
    free(bypass_list);

    unsetenv("NO_PROXY");
    setenv("no_proxy", "", 1);
    bypass_list = proxy_config_env_get_bypass_list();
    EXPECT_EQ(bypass_list, nullptr);
    free(bypass_list);
}
