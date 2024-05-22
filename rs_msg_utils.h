#ifndef RS_MSG_UTILS_H_
#define RS_MSG_UTILS_H_

//#define MIN(x, y) (((x) < (y)) ? (x) : (y))

bool rsm_get_revision(char *dest, const char *src, int src_len);
int rsm_get_boardcpu(char *dest, int dest_len, const char *src, int src_len);

char *rsm_charafterstr(const char *haystack, const char *needle, int length);

bool rsm_isempy(const char *src, int length);
char *rsm_strstr(const char *haystack, const char *needle);
//char *rsm_strnstr(const char *haystack, const char *needle, int length);
char *rsm_strnstr(const char *haystack, const char *needle, size_t slen);
char *rsm_strncasestr(const char *haystack, const char *needle, size_t length);
char *rsm_lastindexof(const char *haystack, const char *needle, size_t length);

int rsm_strmatch(const char *haystack, const char *needle);
int rsm_strmatch_ignore(const char *haystack, const char *needle, int ignore_chars);

int rsm_strncpy(char *dest, const unsigned char *src, int dest_len, int src_len);
int rsm_strcmp(const char *s1, const char *s2);
int rsm_strncmp(const char *haystack, const char *needle, int length);
int rsm_strncpy_nul2sp(char *dest, const unsigned char *src, int dest_len, int src_len);
int rsm_atoi(const char* str);
float rsm_atof(const char* str);
char *rsm_strncpycut(char *dest, const char *src, int dest_len, int src_len);
//char *rsm_char_replace(char *replaced , char *search, const char find, const char replace);
char *rsm_char_replace(char *replaced , char *search,  char *find,  char *replace);
int rsm_HHMM2min(char *message);

#endif //RS_MSG_UTILS_H_
