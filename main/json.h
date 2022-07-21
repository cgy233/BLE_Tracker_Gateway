#ifndef __json__header__h__
#define __json__header__h__

//-------------------------------------------------------------------
char buffer[800] = {0};
char buffer_swap[64] = {0};
static bool is_json_start = false;
static short n_json_put_obj = 0;
static short n_json_put_array = 0;
//-------------------------------------------------------------------
static bool json_start()
{
	if(is_json_start){
		return false;
	}
	memset(buffer, 0, 800);
	buffer[0] = '{';
	is_json_start = true;

	return is_json_start;
}
//-------------------------------------------------------------------
/*
	name=null, data={, else data="name":{
static int json_put_object_start(char *name)
{
	if(!is_json_start){
		return 1;
	}
	memset(buffer_swap, 0, 32);
	if(!name || 0 == *name){
		strcat(buffer, "{");
	}
	else{
		sprintf(buffer_swap, "\"%s\":{", name);
		strcat(buffer, buffer_swap);
	}
	n_json_put_obj += 1;

	return 0;
}
*/
/*
static int json_put_object_end()
{
	if(!is_json_start){
		return 1;
	}
	if(!n_json_put_obj){
		return 2;
	}
	n_json_put_obj -= 1;
	strcat(buffer, "}");
	return 0;
}
*/
/*
	name=null, data=[, else data="name":[
static int json_put_array_start(char *name)
{
	if(!is_json_start){
		return 1;
	}
	if(!name || 0 == *name){
		strcat(buffer, "[");
	}
	else{
		sprintf(buffer_swap, "\"%s\":[", name);
		strcat(buffer, buffer_swap);
	}
	n_json_put_array += 1;

	return 0;
}
*/
/*
static int json_put_array_end()
{
	if(!is_json_start){
		return 1;
	}
	if(!n_json_put_array){
		return 2;
	}
	n_json_put_array -= 1;
	strcat(buffer, "]");
	return 0;
}
*/
/*
*/
static int json_split()
{
	if(!is_json_start){
		return 1;
	}
	strcat(buffer, ",");
	return 0;
}
//-------------------------------------------------------------------
/*
*/
static int json_put_string(char *name, char *value)
{
	if(!is_json_start){
		return 1;
	}
	sprintf(buffer_swap, "\"%s\":\"%s\"", name, value);
	strcat(buffer, buffer_swap);
	return 0;
}
/*
*/
static int json_put_int(char *name, long value)
{
	if(!is_json_start){
		return 1;
	}
	sprintf(buffer_swap, "\"%s\":%ld", name, value);
	strcat(buffer, buffer_swap);

	return 0;
}
/*
static int json_put_bool(char *name, bool value)
{
	if(!is_json_start){
		return 1;
	}
	sprintf(buffer_swap, "\"%s\":%s", name, value? "true":"false");
	strcat(buffer, buffer_swap);

	return 0;
}
*/
//-------------------------------------------------------------------
/*
static int json_put_only_string(char *value)
{
	if(!is_json_start){
		return 1;
	}
	sprintf(buffer_swap, "\"%s\"", value);
	strcat(buffer, buffer_swap);

	return 0;
}
*/
/*
static int json_put_only_int(long value)
{
	if(!is_json_start){
		return 1;
	}
	sprintf(buffer_swap, "%ld", value);
	strcat(buffer, buffer_swap);

	return 0;
}
*/
/*
static int json_put_only_bool( bool value)
{
	if(!is_json_start){
		return 1;
	}
	sprintf(buffer_swap, "%s", value? "true":"false");
	strcat(buffer, buffer_swap);
	return 0;
}
*/
//-------------------------------------------------------------------
/*
*/
static bool json_end()
{
	if(!is_json_start){
		return false;
	}
	buffer[strlen(buffer)] = '}';
	is_json_start = false;

	n_json_put_obj = 0;
	n_json_put_array = 0;

	return !is_json_start;
}
/*
*/
static char* json_buffer(){
	return buffer;
}
#if 0
//-------------------------------------------------------------------
static char *json_get_str(char *json_data, const char *name)
{
	char *swap = (buffer + 128);
	memset(swap, 0, 64);
	sprintf(swap, "\"%s\"", name);

	// find name like "sn"
	char *ph = strstr(json_data, swap);
	// not find
	if(!ph){
		return 0;
	}
	// offset to name
	ph += strlen(swap);
	// find value start
	char *value = strstr(ph, "\"");
	if(!value){
		return 0;
	}
	// offset
	value += 1;
	// find value end
	ph = strstr(value, "\"");
	if(!ph){
		return 0;
	}
	*ph = 0;
	return value;
}
/*
static char *json_get_array(char *json_data, const char *name)
{
	char *swap = (buffer + 128);
	memset(swap, 0, 64);

	char *ph = json_data;
	if(0 != name && 0 != *name)
	{
		sprintf(swap, "\"%s\"", name);
		ph = strstr(ph, swap);
		// not find
		if(!ph){
			return 0;
		}
		ph += strlen(swap);
	}
	// find name like "["
	ph = strstr(ph, "[");
	return (ph? (ph + 1) : 0);
}
*/
/*
static char *json_get_object(char *json_data, const char *name)
{
	char *swap = (buffer + 128);
	memset(swap, 0, 64);
	char *ph = json_data;
	if(0 != name && 0 != *name)
	{
		sprintf(swap, "\"%s\"", name);
		ph = strstr(ph, swap);
		// not find
		if(!ph){
			return 0;
		}
		ph += strlen(swap);
	}
	// find name like "{"
	ph = strstr(ph, "{");
	return (ph? (ph + 1) : 0);
}
*/
/*
	can't be <0, default return -1
*/
static char* json_get_uint(char *json_data, const char *name)
{
	char *swap = (buffer + 128);
	memset(swap, 0, 64);
	sprintf(swap, "\"%s\"", name);

	// find name like "sn"
	char *ph = strstr(json_data, swap);
	// not find
	if(!ph){
		return 0;
	}
	// offset to name
	ph += strlen(swap);
	// find value start
	char *value = strstr(ph, ":");
	if(0 == value){
		return 0;
	}
	// offset
	value += 1;

	// find value start
	int i = 0;
	bool find = false;
	for(i = 0; i < 128; i ++)
	{
		if(*value >= '0' && *value <='9')
		{
			find = true;
			break;
		}
		++ value;
	}
	if(!find){
		return 0;
	}
	// find value end
	ph = value;
	find = false;
	for(i = 0; i < 24; i ++)
	{
		if(*ph < '0' || *ph > '9')
		{
			find = true;
			break;
		}
		++ ph;
	}
	if(!find){
		return 0;
	}
	*ph = 0;
	return value;
}
#endif
/*
static char *json_get_bool(char *json_data, const char *name)
{
	char *swap = (buffer + 128);
	memset(swap, 0, 64);
	sprintf(swap, "\"%s\"", name);

	// find name like "sn"
	char *ph = strstr(json_data, swap);
	// not find
	if(!ph){
		return 0;
	}
	// offset to name
	ph += strlen(swap);
	// find value start
	char *value = strstr(ph, ":");
	if(!value){
		return 0;
	}
	// offset
	value += 1;

	int i = 0;
	// find value start
	for(i = 0; i < 32; i ++)
	{
		// true
		if(*value == 't' 
			&& *(value+1) == 'r'
			&& *(value+2) == 'u'
			&& *(value+3) == 'e')
		{
			value[4] = 0;
			return value;
		}
		// false
		if(*value == 'f' 
			&& *(value+1) == 'a'
			&& *(value+2) == 'l'
			&& *(value+3) == 's'
			&& *(value+4) == 'e')
		{
			value[5] = 0;
			return value;
		}
		++ value;
	}
	return 0;
}
*/
//-------------------------------------------------------------------
#endif