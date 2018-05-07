#include "utils_cmd_putinsight.h"
#include "scribe_capi.h"
#include "common.h"

#define print_to_socket(fh, ...)                                               \
  do {                                                                         \
    if (fprintf(fh, __VA_ARGS__) < 0) {                                        \
      char errbuf[1024];                                                       \
      WARNING("handle_putnotif: failed to write to socket #%i: %s",            \
              fileno(fh), sstrerror(errno, errbuf, sizeof(errbuf)));           \
      return;                                                                  \
    }                                                                          \
    fflush(fh);                                                                \
  } while (0)


void cmd_handle_putinsight(FILE *fh, char *buffer)
{
    if (is_scribe_initialized()) {
        scribe_log(buffer, "insight");
        print_to_socket(fh, "1 Insight added\n");
    } else {
        print_to_socket(fh, "-1 No Scribe\n");
    }
}

void cmd_handle_reloadinsights(FILE *fh)
{
    if (is_scribe_initialized()) {
        reinitialize_scribe();
        print_to_socket(fh, "1 Insights reloaded\n");
    } else {
        print_to_socket(fh, "-1 No Scribe\n");
    }
}