#ifndef APP_WIFI_PORTAL_H
#define APP_WIFI_PORTAL_H

#include "sdkconfig.h"

#ifdef CONFIG_WIFI_PROVISIONING_PORTAL

/* Blocks in the caller task: brings up the configuration AP
 * (SSID <client_id>-setup, password from the last 3 MAC bytes),
 * serves the web page, saves validated credentials to nvs and
 * restarts. Also restarts after a 10 minute inactivity timeout. */
void wifi_portal_start(void);

#endif /* CONFIG_WIFI_PROVISIONING_PORTAL */

#endif /* APP_WIFI_PORTAL_H */
