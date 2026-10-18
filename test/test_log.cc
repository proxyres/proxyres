#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "log.h"

static std::thread::id log_thread_id;
static std::string last_message;

static void log_message(const char *fmt, va_list args) {
    // Ignore messages logged by resolver threads in the background
    if (std::this_thread::get_id() != log_thread_id)
        return;
    char message[256];
    vsnprintf(message, sizeof(message), fmt, args);
    last_message = message;
}

TEST(log, levels) {
    log_thread_id = std::this_thread::get_id();
    proxy_log_set_error_cb(log_message);
    proxy_log_set_warn_cb(log_message);
    proxy_log_set_info_cb(log_message);
    proxy_log_set_debug_cb(log_message);

    log_error("error %d", 1);
    EXPECT_EQ(last_message, "error 1");
    log_warn("warn %d", 2);
    EXPECT_EQ(last_message, "warn 2");
    log_info("info %d", 3);
    EXPECT_EQ(last_message, "info 3");
    log_debug("debug %d", 4);
    EXPECT_EQ(last_message, "debug 4");

    proxy_log_set_error_cb(NULL);
    proxy_log_set_warn_cb(NULL);
    proxy_log_set_info_cb(NULL);
    proxy_log_set_debug_cb(NULL);

    // Warnings without a callback are printed instead
    log_warn("printed warning");
    EXPECT_EQ(last_message, "debug 4");
}
