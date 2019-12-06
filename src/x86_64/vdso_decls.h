#pragma once

#include <attributes.h>

#define VVAR_DEF(type, name) type name
#define VVAR_REF(name) __vdso_ ## name
#define VVAR_DECL(type, name) extern VVAR type __vdso_ ## name
