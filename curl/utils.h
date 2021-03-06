/*
 * Copyright (C) 2016 Tarantool AUTHORS: please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef DRIVER_UTILS_H
#define DRIVER_UTILS_H 1

#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <tarantool/module.h>


#ifndef TIMEOUT_INFINITY
#  define TIMEOUT_INFINITY ((size_t)-1)
#endif /*TIMEOUT_INFINITY*/


static inline
int
make_str_result(lua_State *L, bool ok, const char *str)
{
	lua_pushboolean(L, ok);
	lua_pushstring(L, str);
	return 2;
}

static inline
int
make_int_result(lua_State *L, bool ok, int i)
{
	lua_pushboolean(L, ok);
	lua_pushinteger(L, i);
	return 2;
}

static inline
int
make_errorno_result(lua_State *L, int the_errno)
{
	lua_pushboolean(L, false);
	lua_pushstring(L, strerror(the_errno));
	return 2;
}

#endif /* DRIVER_UTILS_H */
