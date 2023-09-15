#ifndef OPKG_MANAGE_H__
#define OPKG_MANAGE_H__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "gjson.h"


bool opkgConfigApp(json_object *obj, const char *app, char *output, int *result);

void opkgServiceRestart(const char *service);


#endif //OPKG_MANAGE_H__


