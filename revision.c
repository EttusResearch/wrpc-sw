
const char *build_revision = GIT_REVISION;

#ifdef CONFIG_DETERMINISTIC_BINARY
const char *build_date = "";
#else
const char *build_date = ", compiled on: " __DATE__ " " __TIME__;
#endif

