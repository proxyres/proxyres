#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Generate NULL-terminated array of WPAD URLs from an FQDN
char **wpad_dns_get_urls(const char *fqdn, int32_t *count);

// Free NULL-terminated array of WPAD URLs
void wpad_dns_free_urls(char **urls);

// Request WPAD script using DNS, optionally returning the url it was fetched from
char *wpad_dns(const char *fqdn, char **url);

#ifdef PROXYRES_TESTING
typedef char *(*wpad_dns_fetch_func)(const char *url, int32_t *error);
void wpad_dns_set_fetch_func(wpad_dns_fetch_func func);
#endif

#ifdef __cplusplus
}
#endif
