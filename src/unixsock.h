#ifndef __UNIXSOCK_H__
#define __UNIXSOCK_H__

#include "configfile.h"

extern int us_init(void);
extern int us_config(const char *key, const char *val);
extern int us_config_complex(oconfig_item_t *ci);
extern int us_shutdown_listener(void);

#endif