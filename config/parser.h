#ifndef __CONFIG_PARSER_H__
#define __CONFIG_PARSER_H__

#include "declarations.h"

Config parse_config(toml_datum_t root_table);
void free_config(Config *config);

#endif // __CONFIG_PARSER_H__
