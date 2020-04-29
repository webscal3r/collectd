/**
 * Insights JSON FORMAT
 **/

#ifndef UTILS_FORMAT_DSE_INSIGHTS_H
#define UTILS_FORMAT_DSE_INSIGHTS_H 1

#include "collectd.h"

#include "plugin.h"

#ifndef JSON_GAUGE_FORMAT
#define JSON_GAUGE_FORMAT GAUGE_FORMAT
#endif

int format_insights_initialize(char *buffer, size_t *ret_buffer_fill,
                               size_t *ret_buffer_free);
int format_insights_value_list(char *buffer, size_t *ret_buffer_fill,
                               size_t *ret_buffer_free, const data_set_t *ds,
                               const value_list_t *vl, int store_rates,
                               char const *const *http_attrs,
                               size_t http_attrs_num, int data_ttl,
                               char const *metrics_prefix, int offset, int limit, 
                               int history_length, gauge_t *history_values);

int format_insights_log(char *buffer, size_t *ret_buffer_fill, size_t *ret_buffer_free,
                        char *logmsg, char *file);

int format_insights_finalize(char *buffer, size_t *ret_buffer_fill,
                             size_t *ret_buffer_free);

#endif /* UTILS_FORMAT_INSIGHTS_H */
