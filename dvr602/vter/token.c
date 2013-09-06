#include <stdlib.h>
#include <string.h>
#include "token.h"

token_t tokinit(const char *source, const char *delimiters)
{
  token_t result; 

  result.source		= source;
  result.delimiters	= delimiters;
  result.result		= (const char *)malloc(strlen(source) + 1);

  return result;
}

const char *toknext(token_t *tok)
{
  size_t n;

  if (!tok || !(tok->source) || !(tok->delimiters) || !(tok->result))
    return NULL;

  tok->source += strspn(tok->source, tok->delimiters);
  n = strcspn(tok->source, tok->delimiters);
  strncpy((char *)(tok->result), tok->source, n);
  *((char *)(tok->result) + n) = '\0';
  tok->source += n;

  return tok->result;
}

void tokend(token_t *tok)
{
  if (tok) {
    if (tok->result)
      free((void *)(tok->result));
    tok->result	= tok->source = tok->delimiters = NULL;
  }
}
