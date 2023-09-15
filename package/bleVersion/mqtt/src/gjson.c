#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <json-c/json.h>

#include "gjson.h"

void gjson_array_add(json_object *obj, json_object *child)
{
	/* TODO */
	assert(obj);

	json_object_array_add(obj,child);
}

void gjson_array_add_string(json_object *obj, const char *str)
{
	/* TODO */
	assert(obj);
	assert(str);

	json_object_array_add(obj,json_object_new_string(str));
}


void gjson_add_string(json_object *obj, const char *key, const char *str)
{
	/* TODO */
	assert(obj);
	assert(key);

	json_object_object_add(obj, key, json_object_new_string(str));
}

void gjson_add_int(json_object *obj, const char *key, int value)
{
	/* TODO */
	assert(obj);
	assert(key);

	json_object_object_add(obj, key, json_object_new_int(value));
}

void gjson_add_boolean(json_object *obj, const char *key, bool bval)
{
	/* TODO */
	assert(obj);
	assert(key);

	json_object_object_add(obj, key, json_object_new_boolean(bval));
}

void gjson_add_double(json_object *obj, const char *key, double dval)
{
	/* TODO */
	assert(obj);
	assert(key);

	json_object_object_add(obj, key, json_object_new_double(dval));
}

void gjson_add_object(json_object *obj, const char *key, json_object *oval)
{
	/* TODO */
	assert(obj);
	assert(key);

	json_object_object_add(obj, key, oval);
}

void gjson_add_array(json_object *obj, const char *key, void *array, int len)
{
	/* TODO */
	assert(obj);
	assert(key);
}

void gjson_del_key(json_object *parent, const char *key)
{
	return json_object_object_del(parent, key);
}

const char * gjson_get_string(json_object *parent, const char *key)
{
	json_object *o = NULL;

	/* TODO */
	assert(parent);
	assert(key);

	if (!json_object_object_get_ex(parent, key, &o)) {
		/* printf("Filed %s does not exist\n", key); */
		return NULL;
	}

	return json_object_get_string(o);
}

int gjson_get_int(json_object *parent, const char *key)
{
	json_object *o = NULL;

	/* TODO */
	assert(parent);
	assert(key);

	if (!json_object_object_get_ex(parent, key, &o)) {
		/* printf("Filed %s does not exist\n", key); */
		return -1;
	}

	return json_object_get_int(o);
}

bool gjson_get_boolean(json_object *parent, const char *key)
{
	json_object *o = NULL;

	/* TODO */
	assert(parent);
	assert(key);

	if (!json_object_object_get_ex(parent, key, &o)) {
		/* printf("Filed %s does not exist\n", key); */
		return false;
	}

	if ( json_object_is_type(o, json_type_boolean) ) {
		return json_object_get_boolean(o);
	}

	if ( json_object_is_type(o, json_type_string) ) {
		return (strcmp(json_object_get_string(o), "true") == 0 || strcmp(json_object_get_string(o), "1") == 0);
	}

	return false;
	/*return json_object_get_boolean(o);*/
}

double gjson_get_double(json_object *parent, const char *key)
{
	json_object *o = NULL;

	/* TODO */
	assert(parent);
	assert(key);

	if (!json_object_object_get_ex(parent, key, &o)) {
		/* printf("Filed %s does not exist\n", key); */
		return -1;
	}

	return json_object_get_double(o);
}

json_object *gjson_get_object(json_object *parent, const char *key)
{
	json_object *o = NULL;

	/* TODO */
	assert(parent);
	assert(key);

	if (!json_object_object_get_ex(parent, key, &o)) {
		/* printf("Filed %s does not exist\n", key); */
		return NULL;
	}

	return json_object_get(o);
}

void* gjson_get_array(json_object *parent, const char *key)
{
	/* TODO */
	assert(parent);
	assert(key);
}

int gjson_set_string(json_object *parent, const char *key, const char *str)
{
	/* TODO */
	assert(parent);
	assert(key);

	gjson_del_key(parent, key);
	gjson_add_string(parent, key, str);

	return 0;
}

int gjson_set_int(json_object *parent, const char *key, int value)
{
	/* TODO */
	assert(parent);
	assert(key);

	gjson_del_key(parent, key);
	gjson_add_int(parent, key, value);

	return 0;
}

int gjson_set_boolean(json_object *parent, const char *key, bool new_value)
{
	/* TODO */
	assert(parent);
	assert(key);

	gjson_del_key(parent, key);
	gjson_add_boolean(parent, key, new_value);

	return 0;
}

int gjson_set_double(json_object *parent, const char *key, double dval)
{
	/* TODO */
	assert(parent);
	assert(key);

	gjson_del_key(parent, key);
	gjson_add_double(parent, key, dval);

	return 0;
}

int gjson_set_object(json_object *parent, const char *key, json_object *oval)
{
	/* TODO */
	assert(parent);
	assert(key);

	gjson_del_key(parent, key);
	gjson_add_object(parent, key, oval);

	return 0;
}

int gjson_set_array(json_object *parent, const char *key, void *array, int len)
{
	/* TODO */
	assert(parent);
	assert(key);
	assert(array);

	return 0;
}

int gjson_parse_file(const char *filename, json_object **obj)
{
	int fd = 0;
	long size = 0;
	ssize_t bytes = 0;
	char *data = NULL;

	/* json_object *o = NULL; */

	/* TODO */
	assert(filename);

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		return -1;
	}

	size = lseek(fd, 0, SEEK_END);
	if (size < 0) {
		goto error;
	}
	lseek(fd, 0, SEEK_SET);

	data = (char *)malloc(size + 1);
	if (data == NULL) {
		goto error;
	}

	bytes = read(fd, data, size);
	if (bytes < 0) {
		goto err_free;
	}

	/* o = json_object_new_object(); */
	/* o = json_tokener_parse(data); */
	*obj = json_tokener_parse(data);

	/* *obj = o; */

	/* release something */
	if (data != NULL) {
		free(data);
	}
	close(fd);

	return 0;

err_free:
	if (data != NULL) {
		free(data);
	}

error:
	close(fd);
	return -1;
}

int gjson_save_file(const char *filename, json_object *obj)
{
	int fd = 0;
	int ret = 0;
	ssize_t len = 0;
	ssize_t bytes = 0;

	/* TODO */
	assert(filename);

	/* if file exist, remove it */
	ret = access(filename, F_OK);
	if (ret >= 0) {
		ret = unlink(filename);
		if (ret < 0) {
			return -1;
		}
	}

	fd = open(filename, O_WRONLY | O_CREAT | O_EXCL, 0666);
	if (fd < 0) {
		return -1;
	}

	const char * data = json_object_to_json_string(obj);
	len = strlen(data);
	bytes = write(fd, data, len);
	if (bytes < 0) {
		goto error;
	}

	close(fd);
	return 0;

error:
	close(fd);
	return -1;
}

int gjson_contain_key(json_object *parent, char *key)
{
	assert(parent != NULL);

	json_object *o = NULL;
	return json_object_object_get_ex(parent, key, &o);
}

int gjson_parameter_exist(json_object *input, char **parameters, int param_num)
{
	int i = 0;

	for (i = 0; i < param_num; i++) {
		if (!gjson_contain_key(input, parameters[i])) {
			char err[128] = {0};
			sprintf(err, "%s missing", parameters[i]);
			return -1;
		}
	}

	return 0;
}
