#ifndef APREQ_COOKIE_H
#define APREQ_COOKIE_H

#include "apreq.h"
#include "apr_tables.h"

#ifdef  __cplusplus
extern "C" {
#endif 

typedef struct apreq_jar_t {
    apr_table_t   *cookies;
    apr_pool_t    *pool;
    void          *env;
} apreq_jar_t;

typedef enum { NETSCAPE, RFC } apreq_cookie_version_t;

#define APREQ_COOKIE_VERSION               NETSCAPE
#define APREQ_COOKIE_LENGTH                4096

typedef struct apreq_cookie_t {

    apreq_cookie_version_t version;

    char           *path;
    char           *domain;
    char           *port;
    unsigned        secure;

    char           *comment;
    char           *commentURL;

    union {
        apr_int64_t   max_age; 
        const char   *expires; 
    } time;

    apreq_value_t   v;           /* "raw" value (extended struct) */

} apreq_cookie_t;


#define apreq_value_to_cookie(ptr) apreq_attr_to_type(apreq_cookie_t, \
                                                      v, ptr)
#define apreq_cookie_name(c)  ((c)->v.name)
#define apreq_cookie_value(c) ((c)->v.data)

#define apreq_jar_items(j) apr_table_nelts(j->cookies)
#define apreq_jar_nelts(j) apr_table_nelts(j->cookies)

/**
 * Fetches a cookie from the jar
 *
 * @param jar   The cookie jar.
 * @param name  The name of the desired cookie.
 */

APREQ_DECLARE(apreq_cookie_t *)apreq_cookie(const apreq_jar_t *jar,
                                            const char *name);
#define apreq_cookie(j,k) apreq_value_to_cookie(apreq_char_to_value( \
                              apr_table_get((j)->cookies,k)))

/**
 * Adds a cookie by pushing it to the bottom of the jar.
 *
 * @param jar The cookie jar.
 * @param c The cookie to add.
 */

APREQ_DECLARE(void) apreq_add_cookie(apreq_jar_t *jar, 
                                     const apreq_cookie_t *c);
#define apreq_add_cookie(j,c) apr_table_addn((j)->cookies,\
                                            (c)->v.name,(c)->v.data)

/**
 * Parse the incoming "Cookie:" headers into a cookie jar.
 * 
 * @param env The current environment.
 * @param hdr  String to parse as a HTTP-merged "Cookie" header.
 * @remark "data = NULL" has special behavior.  In this case,
 * apreq_jar(env,NULL) will attempt to fetch a cached object from the
 * environment via apreq_env_jar.  Failing that, it will replace
 * "hdr" with the result of apreq_env_cookie(env), parse that,
 * and store the resulting object back within the environment.
 * This maneuver is designed to mimimize parsing work,
 * since generating the cookie jar is relatively expensive.
 *
 */


APREQ_DECLARE(apreq_jar_t *) apreq_jar(void *env, const char *hdr);

/**
 * Returns a new cookie, made from the argument list.
 * The cookie is allocated from the ctx pool.
 *
 * @param ctx   The current context.
 * @param name  The cookie's name.
 * @param nlen  Length of name.
 * @param value The cookie's value.
 * @param vlen  Length of value.
 */
APREQ_DECLARE(apreq_cookie_t *) apreq_make_cookie(apr_pool_t *pool, 
                                  const char *name, const apr_size_t nlen, 
                                  const char *value, const apr_size_t vlen);


APREQ_DECLARE(apr_status_t) apreq_cookie_attr(apr_pool_t *p,
                                              apreq_cookie_t *c, 
                                              char *attr,
                                              char *val);


/**
 * Returns a string that represents the cookie as it would appear 
 * in a valid "Set-Cookie*" header.
 *
 * @param c The cookie.
 * @param p The pool.
 */
APREQ_DECLARE(char*) apreq_cookie_as_string(apr_pool_t *p,
                                            const apreq_cookie_t *c);

/**
 * Same functionality as apreq_cookie_as_string.  Stores the string
 * representation in buf, using up to len bytes in buf as storage.
 * The return value has the same semantics as that of apr_snprintf,
 * including the special behavior for a "len = 0" argument.
 *
 * @param c The cookie.
 * @param buf Storage location for the result.
 * @param len Size of buf's storage area. 
 */

APREQ_DECLARE(int) apreq_serialize_cookie(char *buf, apr_size_t len,
                                          const apreq_cookie_t *c);

/**
 * Get/set the "expires" string.  For NETSCAPE cookies, this returns 
 * the date (properly formatted) when the cookie is to expire.
 * For RFC cookies, this function returns NULL.
 * 
 * @param c The cookie.
 * @param time_str If NULL, return the current expiry date. Otherwise
 * replace with this value instead.  The time_str should be in a format
 * that apreq_atod() can understand, namely /[+-]?\d+\s*[YMDhms]/.
 */
APREQ_DECLARE(void) apreq_cookie_expires(apr_pool_t *p, apreq_cookie_t *c, 
                                         const char *time_str);

/**
 * Add the cookie to the outgoing "Set-Cookie" headers.
 *
 * @param c The cookie.
 */
APREQ_DECLARE(apr_status_t) apreq_cookie_bake(const apreq_cookie_t *c, 
                                              void *env);

/* XXX: how about baking whole cookie jars, too ??? */

/**
 * Add the cookie to the outgoing "Set-Cookie2" headers.
 *
 * @param c The cookie.
 */
APREQ_DECLARE(apr_status_t) apreq_cookie_bake2(const apreq_cookie_t *c,
                                               void *env);

APREQ_DECLARE(apreq_cookie_version_t) apreq_ua_cookie_version(void *env);


#ifdef __cplusplus
 }
#endif

#endif /*APREQ_COOKIE_H*/


