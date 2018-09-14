#ifndef __UNIXSOCK_H__
#define __UNIXSOCK_H__

#include "configfile.h"

int us_init(void);
int us_config_complex(oconfig_item_t *ci);
int us_shutdown_listener(void);

#endif