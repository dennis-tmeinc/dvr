/*
 * config file parser
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parseconfig.h"

struct CFG_ENTRIES {
    int  ent_count;
    char **ent_names;
    char **ent_values;
    int  **ent_seen;
};

struct CFG_SECTIONS {
    int                 sec_count;
    char                **sec_names;
    struct CFG_ENTRIES  **sec_entries;
};

/* ------------------------------------------------------------------------ */

static struct CFG_SECTIONS *c;

/* ------------------------------------------------------------------------ */

#define ALLOC_SIZE 16

static struct CFG_SECTIONS*
cfg_init_sections(void)
{
    struct CFG_SECTIONS *c;
    c = (struct CFG_SECTIONS *)malloc(sizeof(struct CFG_SECTIONS));
    memset(c,0,sizeof(struct CFG_SECTIONS));
    c->sec_names      = (char**)malloc(ALLOC_SIZE*sizeof(char*));
    c->sec_names[0]   = NULL;
    c->sec_entries    = (struct CFG_ENTRIES **)malloc(ALLOC_SIZE*sizeof(struct CFG_ENTRIES*));
    c->sec_entries[0] = NULL;
    return c;
}

static struct CFG_ENTRIES*
cfg_init_entries(void)
{
    struct CFG_ENTRIES *e;
    e = (struct CFG_ENTRIES *)malloc(sizeof(struct CFG_ENTRIES));
    memset(e,0,sizeof(struct CFG_ENTRIES));
    e->ent_names     = (char**)malloc(ALLOC_SIZE*sizeof(char*));
    e->ent_names[0]  = NULL;
    e->ent_values    = (char**)malloc(ALLOC_SIZE*sizeof(char*));
    e->ent_values[0] = NULL;
    e->ent_seen      = (int **)malloc(ALLOC_SIZE*sizeof(int*));
    e->ent_seen[0]   = 0;
    return e;
}

static struct CFG_ENTRIES*
cfg_find_section(struct CFG_SECTIONS *c, char *name)
{
    struct CFG_ENTRIES* e;
    int i;

    for (i = 0; i < c->sec_count; i++)
	if (0 == strcasecmp(c->sec_names[i],name))
	    return c->sec_entries[i];

    /* 404 not found => create a new one */
    if ((c->sec_count % ALLOC_SIZE) == (ALLOC_SIZE-2)) {
	c->sec_names   = (char**)realloc(c->sec_names,(c->sec_count+2+ALLOC_SIZE)*sizeof(char*));
	c->sec_entries = (CFG_ENTRIES**)realloc(c->sec_entries,(c->sec_count+2+ALLOC_SIZE)*sizeof(struct CFG_ENTRIES*));
    }
    e = cfg_init_entries();
    c->sec_names[c->sec_count]   = strdup(name);
    c->sec_entries[c->sec_count] = e;
    c->sec_count++;    
    c->sec_names[c->sec_count]   = NULL;
    c->sec_entries[c->sec_count] = NULL;
    return e;
}

static void
cfg_set_entry(struct CFG_ENTRIES *e, char *name, char *value)
{
    int i;

    for (i = 0; i < e->ent_count; i++)
	if (0 == strcasecmp(e->ent_names[i],name))
	    break;
    if (i == e->ent_count) {
	/* 404 not found => create a new one */
	if ((e->ent_count % ALLOC_SIZE) == (ALLOC_SIZE-2)) {
	    e->ent_names  =(char**)realloc(e->ent_names,(e->ent_count+2+ALLOC_SIZE)*sizeof(char*));
	    e->ent_values = (char**)realloc(e->ent_values,(e->ent_count+2+ALLOC_SIZE)*sizeof(char*));
	    e->ent_seen   = (int**)realloc(e->ent_seen,(e->ent_count+2+ALLOC_SIZE)*sizeof(int*));
	}
	e->ent_count++;    
	e->ent_names[e->ent_count]  = NULL;
	e->ent_values[e->ent_count] = NULL;
	e->ent_seen[e->ent_count]   = 0;
    }
    e->ent_names[i]  = strdup(name);
    e->ent_values[i] = strdup(value);
}

/* ------------------------------------------------------------------------ */

int
cfg_parse_file(char *filename)
{
    struct CFG_ENTRIES *e = NULL;
    char line[256],tag[64],value[192];
    FILE *fp;
    int nr;
    
    if (NULL == c)
	c = cfg_init_sections();
    if (NULL == (fp = fopen(filename,"r")))
	return -1;

    nr = 0;
    while (NULL != fgets(line,255,fp)) {
	nr++;
	if (line[0] == '\n' || line[0] == '#' || line[0] == '%')
	    continue;
	if (1 == sscanf(line,"[%99[^]]]",value)) {
	    /* [section] */
	    e = cfg_find_section(c,value);
	} else if (2 == sscanf(line," %63[^= ] = %191[^\n]",tag,value)) {
	    /* foo = bar */
	    if (NULL == e) {
		fprintf(stderr,"%s:%d: error: no section\n",filename,nr);
	    } else {
		char *c = value + strlen(value)-1;
		while (c > value  &&  (*c == ' ' || *c == '\t'))
		    *(c--) = 0;
		cfg_set_entry(e,tag,value);
	    }
	} else {
	    /* Huh ? */
	  fprintf(stderr,"%s:%d: syntax error(%s)\n",filename,nr,line);
	}
    }
    fclose(fp);
    return 0;
}

/* ------------------------------------------------------------------------ */

void
cfg_parse_option(char *section, char *tag, char *value)
{
    struct CFG_ENTRIES *e = NULL;
    
    if (NULL == c)
	c = cfg_init_sections();
    e = cfg_find_section(c,section);
    cfg_set_entry(e,tag,value);
}

void
cfg_parse_options(int *argc, char **argv)
{
    char section[64], tag[64];
    int i,j;

    for (i = 1; i+1 < *argc;) {
	if (2 == sscanf(argv[i],"-%63[^:]:%63s",section,tag)) {
	    cfg_parse_option(section,tag,argv[i+1]);
	    for (j = i; j < *argc-1; j++)
		argv[j] = argv[j+2];
	    (*argc) -= 2;
	} else {
	    i++;
	}
    }
}

/* ------------------------------------------------------------------------ */

char**
cfg_list_sections()
{
    return c->sec_names;
}

char**
cfg_list_entries(char *name)
{
    int i;

    if (NULL == c)
	return NULL;
    for (i = 0; i < c->sec_count; i++)
	if (0 == strcasecmp(c->sec_names[i],name))
	    return c->sec_entries[i]->ent_names;
    return NULL;
}

char*
cfg_get_str(char *sec, char *ent)
{
    struct CFG_ENTRIES* e = NULL;
    char *v = NULL;
    int i;
    
    for (i = 0; i < c->sec_count; i++)
	if (0 == strcasecmp(c->sec_names[i],sec))
	    e = c->sec_entries[i];
    if (NULL == e)
	return NULL;
    for (i = 0; i < e->ent_count; i++)
	if (0 == strcasecmp(e->ent_names[i],ent)) {
	    v = e->ent_values[i];
	    e->ent_seen[i]++;
	}
    return v;
}

int
cfg_get_int(char *sec, char *ent)
{
    char *val;

    val = cfg_get_str(sec,ent);
    if (NULL == val)
	return -1;
    return atoi(val);
}

int
cfg_get_signed_int(char *sec, char *ent)
{
    char *val;

    val = cfg_get_str(sec,ent);
    if (NULL == val)
	return 0;
    return atoi(val);
}

float
cfg_get_float(char *sec, char *ent)
{
    char *val;

    val = cfg_get_str(sec,ent);
    if (NULL == val)
	return -1;
    return atof(val);
}
