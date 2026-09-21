
typedef enum {
  OK = 0,
  NO_UPDATE_NEEDED,
  ERROR_HTTP,
  ERROR_PARSE,
  ERROR_BUFFER_OVERFLOW,
  ERROR_RESPONSE_CODE,
  ERROR_CONTENT_LENGTH,
  ERROR_OPEN_FILE,
  ERROR_CHECKSUM
} res_t;

extern const char* res_string[9];
extern const char* LIBNAME;
extern const char* LIBNAME_LATEST;

res_t upgrade_player(const char* url, const char* current_version);
