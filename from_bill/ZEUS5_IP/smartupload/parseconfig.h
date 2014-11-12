#ifndef _PARSECONFIG_H_
#define _PARSECONFIG_H_
int    cfg_parse_file(char *filename);
void   cfg_parse_option(char *section, char *tag, char *value);
void   cfg_parse_options(int *argc, char **argv);

char** cfg_list_sections(void);
char** cfg_list_entries(char *name);
char*  cfg_get_str(char *sec, char *ent);
int    cfg_get_int(char *sec, char *ent);
int    cfg_get_signed_int(char *sec, char *ent);
float  cfg_get_float(char *sec, char *ent);
#endif
