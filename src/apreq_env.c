/*
**  Copyright 2003-2004  The Apache Software Foundation
**
**  Licensed under the Apache License, Version 2.0 (the "License");
**  you may not use this file except in compliance with the License.
**  You may obtain a copy of the License at
**
**      http://www.apache.org/licenses/LICENSE-2.0
**
**  Unless required by applicable law or agreed to in writing, software
**  distributed under the License is distributed on an "AS IS" BASIS,
**  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
**  See the License for the specific language governing permissions and
**  limitations under the License.
*/

#include "apreq_env.h"
#include "apr_strings.h"
#include "apr_lib.h"
#include "apr_env.h"
#include "apr_file_io.h"



static int has_rfc_cookie(void *ctx, const char *key, const char *val)
{
    const apreq_cookie_t *c = apreq_value_to_cookie(val);

    /* 0 -> non-netscape cookie found, stop.
       1 -> not found, keep going. */

    return c->version == APREQ_COOKIE_VERSION_NETSCAPE;
}

APREQ_DECLARE(apreq_cookie_version_t)
    apreq_ua_cookie_version(apreq_env_handle_t *env)
{

    if (apreq_header_in(env, "Cookie2") == NULL) {
        const apr_table_t *j;

        if (apreq_jar(env, &j) != APR_SUCCESS
            || apr_table_do(has_rfc_cookie, NULL, j, NULL) == 1)
            return APREQ_COOKIE_VERSION_NETSCAPE;

        else
            return APREQ_COOKIE_VERSION_RFC;
    }
    else
        return APREQ_COOKIE_VERSION_RFC;
}


APREQ_DECLARE(apr_status_t) apreq_cookie_bake(const apreq_cookie_t *c,
                                              apreq_env_handle_t *env)
{
    char s[APREQ_COOKIE_MAX_LENGTH];
    int len = apreq_cookie_serialize(c, s, APREQ_COOKIE_MAX_LENGTH);

    if (len >= APREQ_COOKIE_MAX_LENGTH)
        return APREQ_ERROR_OVERLIMIT;

    return apreq_header_out(env, "Set-Cookie", s);
}

APREQ_DECLARE(apr_status_t) apreq_cookie_bake2(const apreq_cookie_t *c,
                                               apreq_env_handle_t *env)
{
    char s[APREQ_COOKIE_MAX_LENGTH];
    int len = apreq_cookie_serialize(c, s, APREQ_COOKIE_MAX_LENGTH);

    if (c->version == APREQ_COOKIE_VERSION_NETSCAPE)
        return APREQ_ERROR_CONFLICT;

    if (len >= APREQ_COOKIE_MAX_LENGTH)
        return APREQ_ERROR_OVERLIMIT;

    return apreq_header_out(env, "Set-Cookie2", s);
}


APREQ_DECLARE(apreq_param_t *)apreq_param(apreq_env_handle_t *env, 
                                          const char *name)
{
    apreq_param_t *param = apreq_args_get(env, name);
    if (param == NULL)
        return apreq_body_get(env, name);
    else
        return param;
}


APREQ_DECLARE(apr_table_t *)apreq_params(apr_pool_t *pool,
                                         apreq_env_handle_t *env)
{
    const apr_table_t *args, *body;

    if (apreq_args(env, &args) == APR_SUCCESS)
        if (apreq_body(env, &body) == APR_SUCCESS)
            return apr_table_overlay(pool, args, body);
        else
            return apr_table_copy(pool, args);
    else
        if (apreq_body(env, &body) == APR_SUCCESS)
            return apr_table_copy(pool, body);
        else
            return NULL;

}


/** @} */
