#ifndef _TOKEN_H_
#define _TOKEN_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const char *result, *source, *delimiters;
} token_t;

token_t tokinit(const char *source, const char *delimiters);
const char *toknext(token_t * tok);
void tokend(token_t *tok);

#ifdef __cplusplus
}
#endif

#endif
