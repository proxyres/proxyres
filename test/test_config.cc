#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gtest/gtest.h>

#include "proxyres.h"

TEST(config, override_auto_config_url) {
    proxy_config_set_auto_config_url_override("http://127.0.0.1:8000/wpad.dat");
    char *auto_config_url = proxy_config_get_auto_config_url();
    ASSERT_NE(auto_config_url, nullptr);
    EXPECT_STREQ(auto_config_url, "http://127.0.0.1:8000/wpad.dat");
    free(auto_config_url);
}

TEST(config, override_proxy) {
    proxy_config_set_proxy_override("http://127.0.0.1:8000/");
    char *proxy = proxy_config_get_proxy("http");
    ASSERT_NE(proxy, nullptr);
    EXPECT_STREQ(proxy, "http://127.0.0.1:8000/");
    free(proxy);
}

TEST(config, override_bypass_list) {
    proxy_config_set_bypass_list_override("<local>");
    char *bypass_list = proxy_config_get_bypass_list();
    ASSERT_NE(bypass_list, nullptr);
    EXPECT_STREQ(bypass_list, "<local>");
    free(bypass_list);
}

TEST(config, override_bypass_list_with_empty_list) {
    proxy_config_set_bypass_list_override("");
    char *bypass_list = proxy_config_get_bypass_list();
    EXPECT_STREQ(bypass_list, "");
    free(bypass_list);  // In case the condition above is not met
}

TEST(config, read_system) {
    // Clear the overrides set by the tests above
    proxy_config_set_auto_config_url_override(NULL);
    proxy_config_set_proxy_override(NULL);
    proxy_config_set_bypass_list_override(NULL);

    printf("auto discover: %d\n", proxy_config_get_auto_discover());
    char *auto_config_url = proxy_config_get_auto_config_url();
    printf("auto config url: %s\n", auto_config_url ? auto_config_url : "(null)");
    free(auto_config_url);
    for (const char *scheme : {"http", "https", "socks", "ftp", "rtsp"}) {
        char *proxy = proxy_config_get_proxy(scheme);
        printf("%s proxy: %s\n", scheme, proxy ? proxy : "(null)");
        free(proxy);
    }
    char *bypass_list = proxy_config_get_bypass_list();
    printf("bypass list: %s\n", bypass_list ? bypass_list : "(null)");
    free(bypass_list);
}
