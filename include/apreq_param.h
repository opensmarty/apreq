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

#ifndef APREQ_PARAM_H
#define APREQ_PARAM_H

#include "apreq_util.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/**
 * @file apreq_params.h
 * @brief Request parsing and parameter API
 * @ingroup libapreq2
 */


/** Common data structure for params and file uploads */
typedef struct apreq_param_t {
    apr_table_t         *info;   /**< header table associated with the param */
    apr_bucket_brigade  *upload; /**< brigade used to spool upload files */
    unsigned char        flags;  /**< charsets, taint marks, app-specific bits */
    const apreq_value_t  v;      /**< underlying name/value/status info */
} apreq_param_t;


/** Upgrades args and body table values to apreq_param_t structs. */
static APR_INLINE
apreq_param_t *apreq_value_to_param(const char *val)
{
    union { const char *in; char *out; } deconst;

    deconst.in = val;
    return apreq_attr_to_type(apreq_param_t, v, 
           apreq_attr_to_type(apreq_value_t, data, deconst.out));
}


static APR_INLINE
const char *apreq_param_name(const apreq_param_t *p) {
    return p->v.name;
}

static APR_INLINE
const char *apreq_param_value(const apreq_param_t *p) {
    return p->v.data;
}

static APR_INLINE
const apr_table_t *apreq_param_info(const apreq_param_t *p) {
    return p->info;
}

static APR_INLINE
apr_bucket_brigade *apreq_param_brigade(const apreq_param_t *p) {
    apr_bucket_brigade *bb;

    if (p->upload == NULL)
        return NULL;

    bb = apr_brigade_create(p->upload->p, p->upload->bucket_alloc);
    apreq_brigade_copy(bb, p->upload);

    return bb;
}

/** creates a param from name/value information */
APREQ_DECLARE(apreq_param_t *) apreq_make_param(apr_pool_t *p, 
                                                const char *name, 
                                                const apr_size_t nlen, 
                                                const char *val, 
                                                const apr_size_t vlen);

/**
 * Url-decodes a name=value pair into a param.
 * @param pool  Pool from which the param is allocated.
 * @param word  Start of the name=value pair.
 * @param nlen  Length of urlencoded name.
 * @param vlen  Length of urlencoded value.
 * @remark      Unless vlen == 0, this function assumes there is
 *              exactly one character ('=') which separates the pair.
 *            
 */
APREQ_DECLARE(apr_status_t) apreq_decode_param(apreq_param_t **param,
                                               apr_pool_t *pool, 
                                               const char *word,
                                               const apr_size_t nlen, 
                                               const apr_size_t vlen);
/**
 * Url-encodes the param into a name-value pair.
 * @param pool Pool which allocates the returned string.
 * @param param Param to encode.
 * @return name-value pair representing the param.
 */

APREQ_DECLARE(char *) apreq_encode_param(apr_pool_t *pool, 
                                         const apreq_param_t *param);

/**
 * Parse a url-encoded string into a param table.
 * @param pool    pool used to allocate the param data.
 * @param t       table to which the params are added.
 * @param qs      Query string to url-decode.
 * @return        APR_SUCCESS if successful, error otherwise.
 * @remark        This function uses [&;] as the set of tokens
 *                to delineate words, and will treat a word w/o '='
 *                as a name-value pair with value-length = 0.
 *
 */

APREQ_DECLARE(apr_status_t) apreq_parse_query_string(apr_pool_t *pool,
                                                     apr_table_t *t, 
                                                     const char *qs);



/**
 * Returns an array of parameters (apreq_param_t *) matching the given key.
 * The key is case-insensitive.
 * @param p Allocates the returned array.
 * @param req The current apreq_request_t object.
 * @param key Null-terminated search key.  key==NULL fetches all parameters.
 * @remark Also parses the request if necessary.
 */
APREQ_DECLARE(apr_array_header_t *) apreq_params_as_array(apr_pool_t *p,
                                                          const apr_table_t *t,
                                                          const char *key);

/**
 * Returns a ", " -separated string containing all parameters 
 * for the requested key, NULL if none are found.  The key is case-insensitive.
 * @param p Allocates the return string.
 * @param req The current apreq_request_t object.
 * @param key Null-terminated parameter name. key==NULL fetches all values. 
 * @param mode Join type- see apreq_join().
 * @return Returned string is the data attribute of an apreq_value_t,
 *         so it is safe to use in apreq_strlen() and apreq_strtoval().
 * @remark Also parses the request if necessary.
 */
APREQ_DECLARE(const char *) apreq_params_as_string(apr_pool_t *p,
                                                   const apr_table_t *t,
                                                   const char *key,
                                                   apreq_join_t mode);

/**
 * Returns a table of all params in req->body with non-NULL upload brigades.
 * @param pool Pool which allocates the table struct.
 * @param req  Current request.
 * @return Upload table.
 * @remark Will parse the request if necessary.
 */

APREQ_DECLARE(const apr_table_t *)
        apreq_uploads(const apr_table_t *body, apr_pool_t *pool);

/**
 * Returns the first param in req->body which has both param->v.name 
 * matching key and param->upload != NULL.
 * @param req The current request.
 * @param key Parameter name. key == NULL returns first upload.
 * @return Corresponding upload, NULL if none found.
 * @remark Will parse the request as necessary.
 */

APREQ_DECLARE(const apreq_param_t *)
        apreq_upload(const apr_table_t *body, const char *name);


#ifdef __cplusplus
}

#endif
#endif /* APREQ_PARAMS_H */


