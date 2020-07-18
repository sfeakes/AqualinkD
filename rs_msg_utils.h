#ifndef RS_MSG_UTILS_H_
#define RS_MSG_UTILS_H_

int rsm_strncpy(char *dest, const unsigned char *src, int dest_len, int src_len);
int rsm_strcmp(const char *s1, const char *s2);
int rsm_strncpy_nul2sp(char *dest, const unsigned char *src, int dest_len, int src_len);
int rsm_atoi(const char* str);
float rsm_atof(const char* str);

#endif //RS_MSG_UTILS_H_
