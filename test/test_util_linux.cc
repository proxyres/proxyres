#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "util_linux.h"

struct get_config_value_param {
    const char *section;
    const char *key;
    const char *expected;

    friend std::ostream &operator<<(std::ostream &os, const get_config_value_param &param) {
        return os << "section: " << param.section << std::endl << "key: " << param.key;
    }
};

static const char *config = R"(
[Notification Messages]
WarnOnLeaveSSLMode=false

[Proxy Settings]
AuthMode=0
NoProxyFor=
Proxy Config Script=
ProxyType=0
ReversedException=false
ftpProxy=
httpProxy=http://127.0.0.1:8000/
httpsProxy=http://127.0.0.1:8001/)";

constexpr get_config_value_param get_config_value_tests[] = {
    {"Proxy Settings", "ProxyType", "0"},
    {"Proxy Settings", "httpProxy", "http://127.0.0.1:8000/"},
    {"Proxy Settings", "httpsProxy", "http://127.0.0.1:8001/"},
    // No value set should return NULL
    {"Proxy Settings", "ftpProxy", NULL},
    {"Proxy Settings", "AuthMode", "0"},
    {"Proxy Settings", "NoProxyFor", NULL},
    {"Proxy Settings", "Proxy Config Script", NULL},
    {"Proxy Settings", "ReversedException", "false"},
    // No key should return NULL
    {"Proxy Settings", "Does Not Exist", NULL},
    {"Notification Messages", "WarnOnLeaveSSLMode", "false"},
};

class util_config : public ::testing::TestWithParam<get_config_value_param> {};

INSTANTIATE_TEST_SUITE_P(util, util_config, testing::ValuesIn(get_config_value_tests));

TEST_P(util_config, get_value) {
    const auto &param = GetParam();
    char *value = get_config_value(config, param.section, param.key);
    if (value) {
        EXPECT_STREQ(value, param.expected);
        free(value);
    } else {
        EXPECT_EQ(value, param.expected);
    }
}

struct desktop_env_var {
    const char *name;
    const char *value;
};

struct get_desktop_env_param {
    desktop_env_var vars[2];
    int32_t expected;

    friend std::ostream &operator<<(std::ostream &os, const get_desktop_env_param &param) {
        os << "vars:";
        for (const auto &var : param.vars) {
            if (var.name)
                os << " " << var.name << "=" << var.value;
        }
        return os;
    }
};

// GNOME 3 when gsettings is installed, otherwise GNOME 2
constexpr int32_t DESKTOP_ENV_GNOME = -1;

constexpr get_desktop_env_param get_desktop_env_tests[] = {
    {{{"XDG_CURRENT_DESKTOP", "ubuntu:GNOME"}}, DESKTOP_ENV_GNOME3},
    {{{"XDG_CURRENT_DESKTOP", "XFCE"}}, DESKTOP_ENV_GNOME3},
    {{{"XDG_CURRENT_DESKTOP", "KDE"}, {"KDE_SESSION_VERSION", "6"}}, DESKTOP_ENV_KDE5},
    {{{"XDG_CURRENT_DESKTOP", "KDE"}, {"KDE_SESSION_VERSION", "4"}}, DESKTOP_ENV_KDE4},
    {{{"DESKTOP_SESSION", "mate"}}, DESKTOP_ENV_GNOME},
    {{{"DESKTOP_SESSION", "kde-plasma"}}, DESKTOP_ENV_KDE4},
    {{{"KDE_FULL_SESSION", "true"}}, DESKTOP_ENV_KDE3},
    {{}, DESKTOP_ENV_UNKNOWN},
    // Unrecognized desktops fall through to the older variables
    {{{"XDG_CURRENT_DESKTOP", "LXQt"}}, DESKTOP_ENV_UNKNOWN},
    {{{"DESKTOP_SESSION", "lxqt"}}, DESKTOP_ENV_UNKNOWN},
    {{{"DESKTOP_SESSION", "gnome"}}, DESKTOP_ENV_GNOME},
    {{{"GNOME_DESKTOP_SESSION_ID", "this-is-deprecated"}}, DESKTOP_ENV_GNOME},
};

static const char *desktop_env_var_names[] = {"XDG_CURRENT_DESKTOP", "DESKTOP_SESSION", "KDE_SESSION_VERSION",
                                              "KDE_FULL_SESSION", "GNOME_DESKTOP_SESSION_ID"};

class util_desktop_env : public ::testing::TestWithParam<get_desktop_env_param> {
   protected:
    void SetUp() override {
        // Clear the variables of the desktop environment running the tests
        for (const char *name : desktop_env_var_names) {
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

INSTANTIATE_TEST_SUITE_P(util, util_desktop_env, testing::ValuesIn(get_desktop_env_tests));

TEST_P(util_desktop_env, get) {
    const auto &param = GetParam();
    for (const auto &var : param.vars) {
        if (var.name)
            setenv(var.name, var.value, 1);
    }
    int32_t expected = param.expected;
    if (expected == DESKTOP_ENV_GNOME)
        expected = access("/usr/bin/gsettings", F_OK) != -1 ? DESKTOP_ENV_GNOME3 : DESKTOP_ENV_GNOME2;
    EXPECT_EQ(get_desktop_env(), expected);
}
