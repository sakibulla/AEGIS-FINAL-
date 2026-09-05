#ifndef ESPNOW_SLAVE_H
#define ESPNOW_SLAVE_H

#include <esp_err.h>
#include "protocol_defs.h"

#define PATHFINDER_NODE_ID 1
#define WIFI_CHANNEL       6

esp_err_t init_slave_espnow(void);

#endif // ESPNOW_SLAVE_H