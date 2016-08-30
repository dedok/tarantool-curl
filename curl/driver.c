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

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdarg.h>

#include "driver.h"


static
void
check_multi_info(curl_t *ctx)
{
    CURLMsg *msg;
    int msgs_left;

    fprintf(stderr, "REMAINING: %d\n", ctx->still_running);

    while ((msg = curl_multi_info_read(ctx->multi, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE) {
            CURL *easy = msg->easy_handle;
            CURLcode res = msg->data.result;
            fprintf(stderr, "DONE: => (%d)\n", res);
            curl_multi_remove_handle(ctx->multi, easy);
            curl_easy_cleanup(easy);
        }
    }
}


static
void
io_f(va_list ap)
{
    curl_t *ctx = va_arg(ap, curl_t *);

    fiber_set_cancellable(true);

    do {
        for (event_t event; pop_event(ctx, &event);) {

            int sfd = event.sfd;

            switch (event.type) {
            case CURL_POLL_IN:
                coio_wait(sfd, COIO_READ, 0.05);
                curl_multi_socket_action(ctx->multi, sfd,
                                         CURL_POLL_IN, &ctx->still_running);
                break;
            case CURL_POLL_OUT:
                coio_wait(sfd, COIO_WRITE, 0.05);
                curl_multi_socket_action(ctx->multi, sfd,
                                         CURL_POLL_OUT, &ctx->still_running);
                break;
            case CURL_POLL_INOUT:
                coio_wait(sfd, COIO_READ|COIO_WRITE, 0.05);
                curl_multi_socket_action(ctx->multi, sfd,
                                         CURL_POLL_INOUT, &ctx->still_running);
                break;
            case CURL_POLL_REMOVE:
            default:
                break;
            }
        } // for events

        fiber_yield();

    } while (ctx->need_work);
}


static
void
work_f(va_list ap)
{
    curl_t *ctx = va_arg(ap, curl_t *);

//    CURLMcode rc;

    fiber_set_cancellable(true);

    while (ctx->need_work) {

        curl_multi_socket_action(ctx->multi, CURL_SOCKET_TIMEOUT,
                                 0, &ctx->still_running);

        check_multi_info(ctx);

        fiber_sleep(0.01);
    }
}

static
int
prog_cb(void *p,
        double dltotal,
        double dlnow,
        double ult,
        double uln)
{
    (void)p;
    (void)ult;
    (void)uln;
    (void)dlnow;
    (void)dltotal;
    fprintf(stderr, "Progress: (%g/%g)\n", dlnow, dltotal);
    return 0;
}


static
size_t
write_cb(void *ptr, size_t size, size_t nmemb, void *data)
{
  size_t realsize = size * nmemb;
  (void)data;
  (void)ptr;
  return realsize;
}


static
bool
set_headers(curl_t *ctx, CURL *easy, lua_State *L)
{
    (void)ctx;
    (void)easy;

    if (!lua_istable(L, 3)) {
        return false;
    }

    lua_getfield(L, 3, "headers");

    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        const char *header_name = lua_tostring(L, -2);
        const char *value = lua_tostring(L, -1);
        fprintf(stderr, "%s - %s\n", header_name, value);
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return true;
}


static
bool
set_body(curl_t *ctx, CURL *easy, lua_State *L)
{
    (void)ctx;
    (void)easy;
    (void)L;
    return true;
}


static
bool
set_method_type(curl_t *ctx, CURL *easy, lua_State *L)
{
    (void)ctx;
    (void)easy;
    (void)L;
    return true;
}


/*
 */
static
int
async_request(lua_State *L)
{
    curl_t *ctx = curl_get(L);
	const char *url = luaL_checkstring(L, 2);

    CURL *easy = curl_easy_init();
    if (!easy) {
		return luaL_error(L, "curl: curl_easy_init failed!");
    }

    if (!set_method_type(ctx, easy, L)) {
        return luaL_error(L, "curl: can't set method type");
    }

    if (!set_headers(ctx, easy, L)) {
        return luaL_error(L, "curl: can't set headers");
    }

    if (!set_body(ctx, easy, L)) {
        return luaL_error(L, "curl: can't set body");
    }

    curl_easy_setopt(easy, CURLOPT_URL, url);
    curl_easy_setopt(easy, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(easy, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(easy, CURLOPT_PROGRESSFUNCTION, prog_cb);
    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(easy, CURLOPT_LOW_SPEED_TIME, 3L);
    curl_easy_setopt(easy, CURLOPT_LOW_SPEED_LIMIT, 10L);

    return curl_make_result(L,
            CURL_LAST,
            /* Note that the add_handle() will set a
             * time-out to trigger very soon so that
             * the necessary socket_action() call will be called
             * by this app */
            curl_multi_add_handle(ctx->multi, easy));
}



/**
 * Lib functions
 */

static
int
lib_version(lua_State *L)
{
	char version[sizeof("xxx.xxx.xxx-xx.xx")];

	snprintf(version, sizeof(version),
            "%i.%i.%i-%i.%i",
            LIBCURL_VERSION_MAJOR,
            LIBCURL_VERSION_MINOR,
            LIBCURL_VERSION_PATCH,
            0,
            1);

	return make_str_result(L, true, version);
}


static
int
sock_f(CURL *easy, curl_socket_t sfd, int what, void *ctx_, void *tail_ctx)
{
    (void)tail_ctx;
    (void)easy;

    curl_t *ctx = (curl_t*) ctx_;
    event_t event = {.sfd = sfd, .type = what};
    push_event(ctx, event);

    fiber_wakeup(ctx->io_fiber);
    return 0;
}



static
int
lib_new(lua_State *L)
{
    const char *reason = NULL;

	curl_t *ctx = (curl_t *) lua_newuserdata(L, sizeof(curl_t));
	if (!ctx) {
		return luaL_error(L, "curl: lua_newuserdata failed!");
	}

    memset(ctx, 0, sizeof(curl_t));

    ctx->multi = curl_multi_init();
    if (!ctx->multi) {
        reason = "cur; can't create 'multi'";
        goto error;
    }

    ctx->fiber = fiber_new("__curl_fiber", work_f);
    if (!ctx->fiber) {
        reason = "curl can't create new fiber: __curl_fiber";
        goto error;
    }

    ctx->io_fiber = fiber_new("__curl_io_fiber", io_f);
    if (!ctx->io_fiber) {
        reason = "curl can't create new fiber: __curl_io_fiber";
        goto error;
    }

    curl_multi_setopt(ctx->multi, CURLMOPT_SOCKETFUNCTION, sock_f);
    curl_multi_setopt(ctx->multi, CURLMOPT_SOCKETDATA, ctx);

    ctx->need_work = true;

    /* Run fibers */
    fiber_set_joinable(ctx->fiber, true);
    fiber_start(ctx->fiber, ctx);

    fiber_set_joinable(ctx->io_fiber, true);
    fiber_start(ctx->io_fiber, ctx);

    luaL_getmetatable(L, DRIVER_LUA_UDATA_NAME);
	lua_setmetatable(L, -2);

	ctx->L = L;

	return 1;

error:
    if (ctx->multi) {
        curl_multi_cleanup(ctx->multi);
    }
    if (ctx->fiber) {
        fiber_cancel(ctx->fiber);
    }
    if (ctx->io_fiber) {
        fiber_cancel(ctx->io_fiber);
    }

    return luaL_error(L, reason);
}


static
int
curl_destroy(lua_State *L)
{
	curl_t *ctx = curl_get(L);

    ctx->need_work = false;

    if (ctx->fiber) {
        fiber_cancel(ctx->fiber);
        fiber_join(ctx->fiber);
    }

    if (ctx->multi) {
        curl_multi_cleanup(ctx->multi);
    }

	/* remove all methods operating on ctx */
	lua_newtable(L);
	lua_setmetatable(L, -2);

	return make_int_result(L, true, 0);
}



/*
 * Lists of exporting: object and/or functions to the Lua
 */

static const struct luaL_Reg R[] = {
	{"version", lib_version},
	{"new",     lib_new},

	{NULL,      NULL}
};

static const struct luaL_Reg M[] = {

	{"destroy", curl_destroy},
	{"__gc",    curl_destroy},
    {"async_request", async_request},

	{NULL,		NULL}
};

/*
 * ]]
 */


/*
 * Lib initializer
 */
LUA_API
int
luaopen_curl_driver(lua_State *L)
{
	/**
	 * Add metatable.__index = metatable
	 */
	luaL_newmetatable(L, DRIVER_LUA_UDATA_NAME);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	luaL_register(L, NULL, M);
	luaL_register(L, NULL, R);

	/**
	 * Add definitions
	 */
	//register_defs(L, main_defs);

	return 1;
}
