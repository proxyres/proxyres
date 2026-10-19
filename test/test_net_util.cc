#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gtest/gtest.h>

#include "net_util.h"

TEST(net_util, my_ip_address) {
    char *address = my_ip_address();
    EXPECT_NE(address, nullptr);
    if (address) {
        EXPECT_NE(strchr(address, '.'), nullptr);
        EXPECT_EQ(strpbrk(address, ";:"), nullptr);
        free(address);
    }
}

TEST(net_util, my_ip_address_ex) {
    char *addresses = my_ip_address_ex();
    EXPECT_NE(addresses, nullptr);
    if (addresses) {
        EXPECT_NE(strpbrk(addresses, ".:"), nullptr);
        free(addresses);
    }
}

TEST(net_util, dns_resolve_literal) {
    int32_t error = 0;
    char *ip = dns_resolve("127.0.0.1", &error);
    EXPECT_EQ(error, 0);
    EXPECT_STREQ(ip, "127.0.0.1");
    free(ip);

    ip = dns_resolve_ex("::1", &error);
    EXPECT_EQ(error, 0);
    EXPECT_STREQ(ip, "::1");
    free(ip);

    // IPv6 address has no IPv4 equivalent
    ip = dns_resolve("::1", &error);
    EXPECT_EQ(ip, nullptr);
    EXPECT_NE(error, 0);
    free(ip);
}

TEST(net_util, dns_resolve_localhost) {
    int32_t error = 0;
    char *ip = dns_resolve("localhost", &error);
    EXPECT_EQ(error, 0);
    ASSERT_NE(ip, nullptr);
    // Any address in the loopback range
    EXPECT_TRUE(is_ipv4_in_cidr_range(ip, "127.0.0.0/8")) << ip;
    free(ip);
}

TEST(net_util, dns_resolve_google) {
    int32_t error = 0;
    char *ip = dns_resolve("google.com", &error);
    EXPECT_EQ(error, 0);
    EXPECT_NE(ip, nullptr);
    if (ip) {
        EXPECT_NE(strstr(ip, "."), nullptr);
        EXPECT_EQ(strpbrk(ip, ";:"), nullptr);
        free(ip);
    }
}

TEST(net_util, dns_resolve_bad) {
    int32_t error = 0;
    char *ip = dns_resolve("hopefully-doesnt-exist.com", &error);
    EXPECT_EQ(ip, nullptr);
    EXPECT_NE(error, 0);
}

TEST(net_util, dns_ex_resolve_google) {
    int32_t error = 0;
    char *ips = dns_resolve_ex("google.com", &error);
    EXPECT_EQ(error, 0);
    EXPECT_NE(ips, nullptr);
    if (ips) {
        EXPECT_NE(strpbrk(ips, ".:"), nullptr);
        free(ips);
    }
}

TEST(net_util, ipv4_cidr) {
    EXPECT_TRUE(is_ipv4_in_cidr_range("192.167.2.1", "192.168.0.1/8"));
    EXPECT_TRUE(is_ipv4_in_cidr_range("10.1.2.3", "0.0.0.0/0"));
    EXPECT_FALSE(is_ipv4_in_cidr_range("192.168.0.0", "192.168.0.1/32"));
    EXPECT_FALSE(is_ipv4_in_cidr_range("192.168.0.1", "192.168.0.1/33"));
    EXPECT_FALSE(is_ipv4_in_cidr_range("192.168.0.1", "192.168.0.1"));
    EXPECT_FALSE(is_ipv4_in_cidr_range("::1", "192.168.0.1/8"));
    EXPECT_FALSE(is_ipv4_in_cidr_range("192.168.0.1", "invalid/8"));
}

TEST(net_util, ipv6_cidr) {
    EXPECT_TRUE(is_ipv6_in_cidr_range("::ffff:c0a8:1", "::ffff:c0a0:1/108"));
    EXPECT_TRUE(is_ipv6_in_cidr_range("2001:db8::1", "::/0"));
    EXPECT_FALSE(is_ipv6_in_cidr_range("::ffff:c0a8:0", "::ffff:c0a8:1/128"));
    EXPECT_FALSE(is_ipv6_in_cidr_range("::1", "::1/129"));
    EXPECT_FALSE(is_ipv6_in_cidr_range("127.0.0.1", "::1/128"));
    EXPECT_FALSE(is_ipv6_in_cidr_range("::1", "::1"));
    EXPECT_FALSE(is_ipv6_in_cidr_range("::1", "invalid/64"));
}
