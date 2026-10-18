#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Request WPAD url using DHCP with a particular network adapter
char *wpad_dhcp_adapter(uint8_t bind_ip[4], net_adapter_s *adapter, int32_t timeout_sec);

// Check WPAD url from DHCP uses a scheme that is safe to fetch
bool wpad_dhcp_is_url_allowed(const char *url);

// Find WPAD url by enumerating through for all network adapters
char *wpad_dhcp(int32_t timeout_sec);

#ifdef __cplusplus
}
#endif
