#include "curl_update.h"

#include <curl/curl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "jsmn.h"
#include "sha1.h"

#define LOG_PRINTF(f_, ...)                                                                                            \
  {                                                                                                                    \
    printf(log_decorator, getpid());                                                                                   \
    printf((f_), ##__VA_ARGS__);                                                                                       \
  }
extern const char* log_decorator;

const char* res_string[9] = {"OK",
                             "NO_UPDATE_NEEDED",
                             "ERROR_HTTP",
                             "ERROR_PARSE",
                             "ERROR_BUFFER_OVERFLOW",
                             "ERROR_RESPONSE_CODE",
                             "ERROR_CONTENT_LENGTH",
                             "ERROR_OPEN_FILE",
                             "ERROR_CHECKSUM"};

// Resizeable memstruct
// Remember that you are responsible to free(rb->memory);
struct reponse_buffer {
  char* memory;
  size_t size;
  size_t reserved;
};

// Forward declarations
typedef size_t (*write_callback_t)(void*, size_t, size_t, void*);
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp);
static res_t upgrade_fetch_player(const char* url, const char* checksum);

static res_t perform_and_check(CURL* curl, int minimum_expected_size) {
  double content_length = -1;
  long response_code = 0;

  CURLcode res = curl_easy_perform(curl);
  if (CURLE_OK != res) {
    fprintf(stderr, "Curl error %d (%s)\n", res, curl_easy_strerror(res));
    return ERROR_HTTP;
  }

  curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &content_length);
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

  if (response_code < 200 || response_code >= 300) {
    fprintf(stderr, "Bad response code: %ld\n", response_code);
    return ERROR_RESPONSE_CODE;
  }

  // Make sure response is att least 10k.
  if (content_length >= 0 && content_length < minimum_expected_size) {
    fprintf(stderr, "Did only get %d bytes.\n", (int)content_length);
    return ERROR_CONTENT_LENGTH;
  }

  return OK;
}

static res_t upgrade_curl_setup(CURL* curl, const char* url, write_callback_t write_function, char* memory_ptr) {
  struct curl_slist* chunk = NULL;
  chunk = curl_slist_append(chunk, "Accept:");
  chunk = curl_slist_append(chunk, "X-Device-Key-0:eth0");
  // Change to your vendor ID, which SYB will send you
  chunk = curl_slist_append(chunk, "X-Device-Vendor:your_vendor_id");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_function);
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip,deflate");
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, memory_ptr);

  curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
  // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); //Debug information
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  return OK;
}

static res_t upgrade_get_latest_version(const char* url, const char* current_version) {
  struct reponse_buffer resp_buf;
  res_t res = OK;
  char version[512] = "", link[512] = "", checksum[512] = "";
  CURL* curl = curl_easy_init();
  if (!curl)
    return ERROR_HTTP;

  resp_buf.memory = malloc(512); /* will be grown as needed by the realloc above */
  resp_buf.reserved = 512;       /* no data at this point */
  resp_buf.size = 0;

  if ((res = upgrade_curl_setup(curl, url, write_callback, (void*)&resp_buf)) != OK) {
    goto out;
  }

  if ((res = perform_and_check(curl, 128)) != OK) {
    goto out;
  }

  jsmn_parser p;
  jsmntok_t t[128]; /* We expect no more than 128 tokens */

  jsmn_init(&p);
  int r = jsmn_parse(&p, resp_buf.memory, resp_buf.size, t, sizeof(t) / sizeof(t[0]));
  if (r < 0) {
    LOG_PRINTF("Failed to parse JSON: %d\n", r);
    res = ERROR_PARSE;
    goto out;
  }

  /* Assume the top-level element is an object */
  if (r < 1 || t[0].type != JSMN_OBJECT) {
    LOG_PRINTF("Object expected\n");
    res = ERROR_PARSE;
    goto out;
  }

  /* Loop over all keys of the root object */
  int i;
  for (i = 1; i < r; i++) {
    if (jsoneq(resp_buf.memory, &t[i], "version") == 0) {
      if (t[i + 1].end - t[i + 1].start > (int)sizeof(version) - 1) {
        res = ERROR_BUFFER_OVERFLOW;
        goto out;
      }

      /* We may use strndup() to fetch string value */
      strncpy(version, resp_buf.memory + t[i + 1].start, t[i + 1].end - t[i + 1].start);
      i++;
    } else if (jsoneq(resp_buf.memory, &t[i], "link") == 0) {
      if (t[i + 1].end - t[i + 1].start > (int)sizeof(link) - 1) {
        res = ERROR_BUFFER_OVERFLOW;
        goto out;
      }

      /* We may use strndup() to fetch string value */
      strncpy(link, resp_buf.memory + t[i + 1].start, t[i + 1].end - t[i + 1].start);
      i++;
    } else if (jsoneq(resp_buf.memory, &t[i], "checksum") == 0) {
      if (t[i + 1].end - t[i + 1].start > (int)sizeof(link) - 1) {
        res = ERROR_BUFFER_OVERFLOW;
        goto out;
      }

      /* We may use strndup() to fetch string value */
      strncpy(checksum, resp_buf.memory + t[i + 1].start, t[i + 1].end - t[i + 1].start);
      i++;
    }
  }

  LOG_PRINTF("Got from server version: '%s' link: '%s' checksum: '%s'\n", version, link, checksum);

  if (strcmp(version, current_version) != 0) {
    LOG_PRINTF("Will download new version '%s' != '%s'...\n", version, current_version);
    if ((res = upgrade_fetch_player(link, checksum)) != OK) {
      goto out;
    }
  } else {
    LOG_PRINTF("No update availiable...\n");
    res = NO_UPDATE_NEEDED;
  }

out:
  if (resp_buf.memory)
    free(resp_buf.memory);
  if (curl)
    curl_easy_cleanup(curl);
  return res;
}

typedef struct {
  FILE* stream;
  SHA1Context sha;
  int bytes;
} file_and_hash_t;

static size_t write_and_hash_callback(void* contents, size_t size, size_t nmemb, void* userp) {
  file_and_hash_t* ctx = (file_and_hash_t*)userp;
  SHA1Input(&ctx->sha, contents, (unsigned)(size * nmemb));
  ctx->bytes += size * nmemb;
  return fwrite(contents, size, nmemb, ctx->stream);
}

static res_t upgrade_fetch_player(const char* url, const char* checksum) {
  file_and_hash_t ctx = {};
  CURL* curl = NULL;
  res_t res = OK;
  unsigned char buf1[20] = {0};
  unsigned char buf2[20] = {0};

  // Confirm checksum length (40 bytes in ascii, is 20 bytes parsed)
  if (strlen(checksum) != 40) {
    LOG_PRINTF("Invalid checksum from server '%s' len %zu\n", checksum, strlen(checksum));
    res = ERROR_CHECKSUM;
    goto out;
  }

  // Convert string to compare
  for (size_t i = 0; i < sizeof(buf1); i++) {
    if (sscanf(&checksum[i * 2], "%02hhX", &buf2[i]) != 1) {
      LOG_PRINTF("Hex parsed fail at: %zu\n", i);
      res = ERROR_CHECKSUM;
      goto out;
    }
  }

  curl = curl_easy_init();
  if (!curl) {
    res = ERROR_HTTP;
    goto out;
  }

  if (SHA1Reset(&ctx.sha) != shaSuccess) {
    res = ERROR_CHECKSUM;
    goto out;
  }

  unlink(LIBNAME_LATEST);
  ctx.stream = fopen(LIBNAME_LATEST, "wb");
  if (!ctx.stream) {
    fprintf(stderr, "Unable to open file: '%s' \n", LIBNAME_LATEST);
    res = ERROR_OPEN_FILE;
    goto out;
  }
  LOG_PRINTF("Will save into: '%s'\n", LIBNAME_LATEST);

  // Use a helper function that saves file to disk, and hashes it at the same time streaming.
  if ((res = upgrade_curl_setup(curl, url, (void*)write_and_hash_callback, (void*)&ctx)) != OK) {
    fprintf(stderr, "upgrade_curl_setup() failed\n");
    goto out;
  }

  if ((res = perform_and_check(curl, 10240)) != OK) {
    goto out;
  }

  LOG_PRINTF("Downloaded %d bytes\n", ctx.bytes);

  if (SHA1Result(&ctx.sha, buf1) != shaSuccess) {
    LOG_PRINTF("Sha checksum failed\n");
    res = ERROR_CHECKSUM;
    goto out;
  }

  // Make sure checksums match
  if (memcmp(buf1, buf2, sizeof(buf1)) != 0) {
    LOG_PRINTF("Checksum missmatch!\n");
    for (size_t i = 0; i < sizeof(buf1); i++) {
      LOG_PRINTF("%zu %02X %02X\n", i, buf1[i], buf2[i]);
    }
    res = ERROR_CHECKSUM;
    goto out;
  }
  LOG_PRINTF("Checksum matches!\n");

out:
  if (curl)
    curl_easy_cleanup(curl);
  if (ctx.stream)
    fclose(ctx.stream);
  if (res != OK) {
    // Remove file, since it might be corrupt.
    unlink(LIBNAME_LATEST);
  }
  return res;
}

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
  size_t realsize = size * nmemb;
  struct reponse_buffer* mem = (struct reponse_buffer*)userp;

  if (realsize >= (mem->reserved - mem->size)) {
    mem->memory = realloc(mem->memory, mem->size + realsize + 1);
    if (mem->memory == NULL) {
      /* out of memory! */
      LOG_PRINTF("not enough memory (realloc returned NULL)\n");
      return 0;
    }
    mem->reserved = mem->size + realsize + 1;
  }

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;

  return realsize;
}

res_t upgrade_player(const char* url, const char* current_version) {
  curl_global_init(CURL_GLOBAL_ALL);
  res_t retval = upgrade_get_latest_version(url, current_version);
  curl_global_cleanup();
  return retval;
}
