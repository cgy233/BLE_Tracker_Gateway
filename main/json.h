#ifndef __JSON__HEADER__H__
#define __JSON__HEADER__H__

bool json_start();
bool json_end();
int json_split();
int json_put_string(char *name, char *value);
int json_put_int(char *name, long value);
char *json_buffer();

#endif