/**
 *
 **/

#include "collectd.h"

#include "common.h"
#include "plugin.h"

#include "utils_cache.h"
#include "utils_format_dse_insights.h"

/* This is the INSIGHTS format for write_scribe output
 *
 * Target format
 * [
 *   {
 *     "metadata": {
 *       "name": "collectd.vmem",
 *       "timestamp": 1524257264175,
 *       "insightsType": "GAUGE",
 *       "tags": {
 *          "host": "fqdn.domain.tld",
 *          "plugin_instance": "vmpage_number",
 *          "type": "kernel_stack",
 *          "ds": "value"
 *       }
 *     },
 *     "data": {
 *       "value": 9.999999
 *     }
 *   }
 * ]
 */

static int insights_escape_string(char *buffer, size_t buffer_size, /* {{{ */
                              const char *string) {
  size_t dst_pos;

  if ((buffer == NULL) || (string == NULL))
    return -EINVAL;

  if (buffer_size < 3)
    return -ENOMEM;

  dst_pos = 0;

#define BUFFER_ADD(c)                                                          \
  do {                                                                         \
    if (dst_pos >= (buffer_size - 1)) {                                        \
      buffer[buffer_size - 1] = 0;                                             \
      return -ENOMEM;                                                          \
    }                                                                          \
    buffer[dst_pos] = (c);                                                     \
    dst_pos++;                                                                 \
  } while (0)

  /* Escape special characters */
  BUFFER_ADD('"');
  for (size_t src_pos = 0; string[src_pos] != 0; src_pos++) {
    if ((string[src_pos] == '"') || (string[src_pos] == '\\')) {
      BUFFER_ADD('\\');
      BUFFER_ADD(string[src_pos]);
    } else if (string[src_pos] <= 0x001F)
      BUFFER_ADD('?');
    else
      BUFFER_ADD(string[src_pos]);
  } /* for */
  BUFFER_ADD('"');
  buffer[dst_pos] = 0;

#undef BUFFER_ADD

  return 0;
} /* }}} int json_escape_string */


static int meta_to_tags(char *buffer, size_t buffer_size, /* {{{ */
                        meta_data_t *meta, char **keys,
                        size_t keys_num)
{
  size_t offset = 0;
  int status;

  buffer[0] = 0;

#define BUFFER_ADD(...)                                                        \
  do {                                                                         \
    status = snprintf(buffer + offset, buffer_size - offset, __VA_ARGS__);     \
    if (status < 1)                                                            \
      return -1;                                                               \
    else if (((size_t)status) >= (buffer_size - offset))                       \
      return -ENOMEM;                                                          \
    else                                                                       \
      offset += ((size_t)status);                                              \
  } while (0)

  for (size_t i = 0; i < keys_num; ++i) {
    int type;
    char *key = keys[i];

    type = meta_data_type(meta, key);
    if (type == MD_TYPE_STRING) {
      char *value = NULL;
      if (meta_data_get_string(meta, key, &value) == 0) {
        char temp[512] = "";

        status = insights_escape_string(temp, sizeof(temp), value);
        sfree(value);
        if (status != 0)
          return status;

        BUFFER_ADD(",\"%s\":%s", key, temp);
      }
    } else if (type == MD_TYPE_SIGNED_INT) {
      int64_t value = 0;
      if (meta_data_get_signed_int(meta, key, &value) == 0)
        BUFFER_ADD(",\"%s\":%" PRIi64, key, value);
    } else if (type == MD_TYPE_UNSIGNED_INT) {
      uint64_t value = 0;
      if (meta_data_get_unsigned_int(meta, key, &value) == 0)
        BUFFER_ADD(",\"%s\":%" PRIu64, key, value);
    } else if (type == MD_TYPE_DOUBLE) {
      double value = 0.0;
      if (meta_data_get_double(meta, key, &value) == 0)
        BUFFER_ADD(",\"%s\":%f", key, value);
    } else if (type == MD_TYPE_BOOLEAN) {
      _Bool value = 0;
      if (meta_data_get_boolean(meta, key, &value) == 0)
        BUFFER_ADD(",\"%s\":%s", key, value ? "true" : "false");
    }
  } /* for (keys) */

  if (offset == 0)
    return ENOENT;

#undef BUFFER_ADD

  return 0;
}

static int values_to_insights(char *buffer, size_t buffer_size, /* {{{ */
                              const data_set_t *ds, const value_list_t *vl,
                              int store_rates, size_t ds_idx) {
  size_t offset = 0;
  gauge_t *rates = NULL;

  memset(buffer, 0, buffer_size);

#define BUFFER_ADD(...)                                                        \
  do {                                                                         \
    int status;                                                                \
    status = snprintf(buffer + offset, buffer_size - offset, __VA_ARGS__);     \
    if (status < 1) {                                                          \
      sfree(rates);                                                            \
      return -1;                                                               \
    } else if (((size_t)status) >= (buffer_size - offset)) {                   \
      sfree(rates);                                                            \
      return -ENOMEM;                                                          \
    } else                                                                     \
      offset += ((size_t)status);                                              \
  } while (0)

  BUFFER_ADD("{\"value\":");

  if (ds->ds[ds_idx].type == DS_TYPE_GAUGE) {
    if (isfinite(vl->values[ds_idx].gauge)) {
      BUFFER_ADD(JSON_GAUGE_FORMAT, vl->values[ds_idx].gauge);
    } else {
      DEBUG("utils_format_insights: invalid vl->values[ds_idx].gauge for "
            "%s|%s|%s|%s|%s",
            vl->plugin, vl->plugin_instance, vl->type, vl->type_instance,
            ds->ds[ds_idx].name);
      return -1;
    }
  } else if (store_rates) {
    if (rates == NULL)
      rates = uc_get_rate(ds, vl);
    if (rates == NULL) {
      WARNING("utils_format_insights: uc_get_rate failed for %s|%s|%s|%s|%s",
              vl->plugin, vl->plugin_instance, vl->type, vl->type_instance,
              ds->ds[ds_idx].name);

      return -1;
    }

    if (isfinite(rates[ds_idx])) {
      BUFFER_ADD(JSON_GAUGE_FORMAT, rates[ds_idx]);
    } else {
      DEBUG("utils_format_insights: invalid rates[ds_idx] for %s|%s|%s|%s|%s",
              vl->plugin, vl->plugin_instance, vl->type, vl->type_instance,
              ds->ds[ds_idx].name);
      sfree(rates);
      return -1;
    }
  } else if (ds->ds[ds_idx].type == DS_TYPE_COUNTER) {
    BUFFER_ADD("%llu", vl->values[ds_idx].counter);
  } else if (ds->ds[ds_idx].type == DS_TYPE_DERIVE) {
    BUFFER_ADD("%" PRIi64, vl->values[ds_idx].derive);
  } else if (ds->ds[ds_idx].type == DS_TYPE_ABSOLUTE) {
    BUFFER_ADD("%" PRIu64, vl->values[ds_idx].absolute);
  } else {
    ERROR("format_insights: Unknown data source type: %i", ds->ds[ds_idx].type);
    sfree(rates);
    return -1;
  }
  BUFFER_ADD("}");

#undef BUFFER_ADD

  DEBUG("format_insights: values_to_insights: buffer = %s;", buffer);
  sfree(rates);
  return 0;
} /* }}} int values_to_insights */

static int value_list_to_insights(char *buffer, size_t buffer_size, /* {{{ */
                                  const data_set_t *ds, const value_list_t *vl,
                                  int store_rates,
                                  char const *const *http_attrs,
                                  size_t http_attrs_num, int data_ttl,
                                  char const *metrics_prefix, int off, int lim) {
  char temp[512];
  size_t offset = 0;
  int keys_num;
  char **keys;
  int status;

  memset(buffer, 0, buffer_size);

#define BUFFER_ADD(...)                                                        \
  do {                                                                         \
    status = snprintf(buffer + offset, buffer_size - offset, __VA_ARGS__);     \
    if (status < 1)                                                            \
      return -1;                                                               \
    else if (((size_t)status) >= (buffer_size - offset))                       \
      return -ENOMEM;                                                          \
    else                                                                       \
      offset += ((size_t)status);                                              \
  } while (0)

#define BUFFER_ADD_KEYVAL(key, value)                                          \
  do {                                                                         \
    status = insights_escape_string(temp, sizeof(temp), (value));              \
    if (status != 0)                                                           \
      return status;                                                           \
    BUFFER_ADD(",\"%s\": %s", (key), temp);                                    \
  } while (0)

  for (size_t i = off; i < lim; i++) {
    /* All value lists have a leading comma. The first one will be replaced with
     * a square bracket in `format_insights_finalize'. */
    BUFFER_ADD(",{\"metadata\":");
    BUFFER_ADD("{\"name\":\"");

    if (metrics_prefix != NULL) {
      BUFFER_ADD("%s.", metrics_prefix);
    }

    // Avoid use the plugin-instance for the name in the case of DSE.
    // Till we switch to better naming on DSE side
    if (strcasecmp(vl->plugin, "dse") == 0 && strlen(vl->plugin_instance)) {
        BUFFER_ADD("%s", vl->plugin_instance);
    } else {
        BUFFER_ADD("%s", vl->plugin);
    }

    BUFFER_ADD("\", \"timestamp\":%" PRIu64, CDTIME_T_TO_MS(vl->time));
    BUFFER_ADD(", \"insightMappingId\": \"collectd-v1\"");
    BUFFER_ADD(", \"insightType\":\"");

    switch(ds->ds[i].type) {
        case DS_TYPE_COUNTER:
            BUFFER_ADD("COUNTER");
            break;
        case DS_TYPE_GAUGE:
        case DS_TYPE_DERIVE:
        case DS_TYPE_ABSOLUTE:
            BUFFER_ADD("GAUGE");
            break;
        default:
            ERROR("format_insights: Unknown data source type: %i", ds->ds[i].type);
            return -1;
    }

    /*
     * Now adds meta data to metric as tags
     */
    BUFFER_ADD("\", \"tags\":{");

    if (data_ttl != 0)
      BUFFER_ADD("\"ttl\": %i,", data_ttl);

    BUFFER_ADD("\"collectdType\": %i", ds->ds[i].type);
    BUFFER_ADD(",\"host\": \"%s\"", vl->host);

    //Random K/V pairs
    for (size_t j = 0; j < http_attrs_num; j += 2) {
      BUFFER_ADD(", \"%s\":", http_attrs[j]);
      BUFFER_ADD(" \"%s\"", http_attrs[j + 1]);
    }

    //Add specific metadata
    if (vl->meta != NULL) {
        char meta_buffer[buffer_size];
        memset(meta_buffer, 0, sizeof(meta_buffer));

        status = meta_data_toc(vl->meta, &keys);
        if (status <= 0)
            return status;
        keys_num = (size_t)status;

        status = meta_to_tags(meta_buffer, buffer_size, vl->meta, keys, keys_num);

        if (status != 0)
            return status;

        BUFFER_ADD("%s", meta_buffer);
        for (size_t i = 0; i < keys_num; ++i)
            sfree(keys[i]);
    }


    if (strlen(vl->plugin_instance))
        BUFFER_ADD_KEYVAL("plugin_instance", vl->plugin_instance);
    if (strlen(vl->type_instance))
        BUFFER_ADD_KEYVAL("type_instance", vl->type_instance);
    if (ds->ds_num != 1)
        BUFFER_ADD_KEYVAL("ds", ds->ds[i].name);

    BUFFER_ADD_KEYVAL("plugin", vl->plugin);
    BUFFER_ADD_KEYVAL("type", vl->type);
    BUFFER_ADD("}}");

    memset(temp, 0, sizeof(temp));
    status = values_to_insights(temp, sizeof(temp), ds, vl, store_rates, i);
    if (status != 0)
      return status;

    BUFFER_ADD(", \"data\": %s", temp);
    BUFFER_ADD("}");
  } /* for ds->ds_num */


#undef BUFFER_ADD_KEYVAL
#undef BUFFER_ADD

  DEBUG("format_insights: value_list_to_insights: buffer = %s;", buffer);

  return 0;
} /* }}} int value_list_to_insights */

static int format_insights_value_list_nocheck(
    char *buffer, /* {{{ */
    size_t *ret_buffer_fill, size_t *ret_buffer_free, const data_set_t *ds,
    const value_list_t *vl, int store_rates, size_t temp_size,
    char const *const *http_attrs, size_t http_attrs_num, int data_ttl,
    char const *metrics_prefix, int offset, int limit) {
  char temp[temp_size];
  int status;

  status = value_list_to_insights(temp, sizeof(temp), ds, vl, store_rates,
                                  http_attrs, http_attrs_num, data_ttl,
                                  metrics_prefix, offset, limit);
  if (status != 0)
    return status;
  temp_size = strlen(temp);

  memcpy(buffer + (*ret_buffer_fill), temp, temp_size + 1);
  (*ret_buffer_fill) += temp_size;
  (*ret_buffer_free) -= temp_size;

  return 0;
} /* }}} int format_insights_value_list_nocheck */

int format_insights_log_nocheck(
    char *buffer, size_t *ret_buffer_fill, size_t *ret_buffer_free,
    char *logmsg, size_t logmsg_size, char *file, size_t file_size) {

    size_t buffer_size = (*ret_buffer_free);

    char temp[logmsg_size + 512];
    int status;

    memset(temp, 0, sizeof(temp));

    size_t offset = 0;

  #define BUFFER_ADD(...)                                                        \
    do {                                                                         \
      status = snprintf(buffer + offset, buffer_size - offset, __VA_ARGS__);     \
      if (status < 1)                                                            \
        return -1;                                                               \
      else if (((size_t)status) >= (buffer_size - offset))                       \
        return -ENOMEM;                                                          \
      else                                                                       \
        offset += ((size_t)status);                                              \
    } while (0)


    /* All value logs have a leading comma. The first one will be replaced with
     * a square bracket in `format_insights_finalize'. */
    BUFFER_ADD(",{\"metadata\":");
    BUFFER_ADD("{\"name\":\"%s\"", file);
    BUFFER_ADD(", \"timestamp\":%" PRIu64, CDTIME_T_TO_MS(cdtime()));
    BUFFER_ADD(", \"insightType\":\"LOG\"");
      /*
       * Now adds meta data to metric as tags
       */
    BUFFER_ADD(", \"tags\":{");
    BUFFER_ADD("\"host\": \"%s\"", hostname_g);
    BUFFER_ADD("}}");

    status = insights_escape_string(temp, sizeof(temp), (logmsg));
    if (status != 0)
      return status;

    BUFFER_ADD(", \"data\": %s", temp);
    BUFFER_ADD("}");

    (*ret_buffer_fill) += offset;
    (*ret_buffer_free) -= offset;

    return 0;
}

#undef BUFFER_ADD


int format_insights_initialize(char *buffer, /* {{{ */
                               size_t *ret_buffer_fill,
                               size_t *ret_buffer_free) {
  size_t buffer_fill;
  size_t buffer_free;

  if ((buffer == NULL) || (ret_buffer_fill == NULL) ||
      (ret_buffer_free == NULL))
    return -EINVAL;

  buffer_fill = *ret_buffer_fill;
  buffer_free = *ret_buffer_free;

  buffer_free = buffer_fill + buffer_free;
  buffer_fill = 0;

  if (buffer_free < 3)
    return -ENOMEM;

  memset(buffer, 0, buffer_free);
  *ret_buffer_fill = buffer_fill;
  *ret_buffer_free = buffer_free;

  return 0;
} /* }}} int format_insights_initialize */

int format_insights_finalize(char *buffer, /* {{{ */
                             size_t *ret_buffer_fill, size_t *ret_buffer_free) {
  size_t pos;

  if ((buffer == NULL) || (ret_buffer_fill == NULL) ||
      (ret_buffer_free == NULL))
    return -EINVAL;

  if (*ret_buffer_free < 2)
    return -ENOMEM;

  /* Replace the leading comma added in `value_list_to_insights' with a square
   * bracket. */
  if (buffer[0] != ',')
    return -EINVAL;
  buffer[0] = ' ';

  pos = *ret_buffer_fill;
  buffer[pos] = '\n';
  buffer[pos + 1] = 0;

  (*ret_buffer_fill)++;
  (*ret_buffer_free)--;

  return 0;
} /* }}} int format_insights_finalize */

int format_insights_value_list(char *buffer, /* {{{ */
                               size_t *ret_buffer_fill, size_t *ret_buffer_free,
                               const data_set_t *ds, const value_list_t *vl,
                               int store_rates, char const *const *http_attrs,
                               size_t http_attrs_num, int data_ttl,
                               char const *metrics_prefix, int offset, int limit) {
  if ((buffer == NULL) || (ret_buffer_fill == NULL) ||
      (ret_buffer_free == NULL) || (ds == NULL) || (vl == NULL))
    return -EINVAL;

  if (offset >= limit || ds->ds_num < limit)
    return -EINVAL;

  if (*ret_buffer_free < 3)
    return -ENOMEM;

  return format_insights_value_list_nocheck(
      buffer, ret_buffer_fill, ret_buffer_free, ds, vl, store_rates,
      (*ret_buffer_free) - 2, http_attrs, http_attrs_num, data_ttl,
      metrics_prefix, offset, limit);
} /* }}} int format_insights_value_list */


int format_insights_log(char *buffer, size_t *ret_buffer_fill, size_t *ret_buffer_free, char *logmsg, char *file)
{
    if ((buffer == NULL) || (ret_buffer_fill == NULL) ||
        (ret_buffer_free == NULL) || (logmsg == NULL) || file == NULL)
        return -EINVAL;

    if (*ret_buffer_free < 3)
        return -ENOMEM;

    return format_insights_log_nocheck(buffer, ret_buffer_fill, ret_buffer_free, logmsg, strlen(logmsg), file, strlen(file));
}


/* vim: set sw=2 sts=2 et fdm=marker : */
