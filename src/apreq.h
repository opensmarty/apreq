#ifndef APREQ_H
#define APREQ_H

#ifdef  __cplusplus
 extern "C" {
#endif 

#include "apr_tables.h" 
#include "apr_lib.h"
#include "apr_pools.h"
#include "apr_strings.h"

#include <stddef.h>

#define APREQ_DECLARE(d)                d

/* default enctypes */
#define APREQ_URL_ENCTYPE               "application/x-www-form-urlencoded"
#define APREQ_URL_LENGTH                33
#define APREQ_MFD_ENCTYPE               "multipart/form-data"
#define APREQ_MFD_ENCTYPE_LENGTH        19
#define APREQ_XML_ENCTYPE               "application/xml"
#define APREQ_XML_ENCTYPE_LENGTH        15

#define APREQ_EXPIRES_HTTP              0
#define APREQ_EXPIRES_COOKIE            1

#define APREQ_DEFAULT_POST_MAX         -1 /* XXX: unsafe default ??? */
#define APREQ_DEFAULT_NELTS             8

/* string match modes:
 * full match: full needle must be found 
 * partial match: allow for partial matches at the end of haystack
 */
#define APREQ_MATCH_FULL                0
#define APREQ_MATCH_PART                1

/* join modes */
#define APREQ_JOIN_ESCAPE               1
#define APREQ_JOIN_UNESCAPE             2
#define APREQ_JOIN_URLENCODE            3
#define APREQ_JOIN_QUOTE                4

/* status codes */
#define APREQ_OK                        0
#define APREQ_CONTINUE                100
#define APREQ_ERROR                   500


typedef struct apreq_value_t {
    const char          *name;
    apr_size_t           size;
    apr_status_t         status;
    char                 data[1];
} apreq_value_t;

typedef apreq_value_t *(apreq_value_merge_t)(apr_pool_t *p,
                                             const apr_array_header_t *a);
typedef apreq_value_t *(apreq_value_copy_t)(apr_pool_t *p,
                                            const apreq_value_t *v);

#define apreq_attr_to_type(T,A,P) ( (T*) ((char*)(P)-offsetof(T,A)) )
#define apreq_char_to_value(ptr)  apreq_attr_to_type(apreq_value_t, data, ptr)

#define apreq_strtoval(ptr)  apreq_char_to_value(ptr)
#define apreq_strlen(ptr) (apreq_strtoval(ptr)->size)

apreq_value_t *apreq_value_make(apr_pool_t *p, const char *name, 
                                const char *data, const apr_size_t size);
apreq_value_t *apreq_value_copy(apr_pool_t *p, const apreq_value_t *val);
apreq_value_t *apreq_value_merge(apr_pool_t *p, const apr_array_header_t *arr);

APREQ_DECLARE(const char *) apreq_join(apr_pool_t *p, 
                                       const char *sep, 
                                       const apr_array_header_t *arr, 
                                       int mode);

/* XXX: should we drop this and replace it with apreq_index ? */
char *apreq_memmem(char* hay, apr_off_t haylen, 
                   const char* ndl, apr_off_t ndllen, int part);

apr_off_t apreq_index(const char* hay, apr_off_t haylen, 
                      const char* ndl, apr_off_t ndllen, int part);

/* url-escapes non-alphanumeric characters */
apr_size_t apreq_quote(char *dest, const char *src, const apr_size_t slen);
apr_size_t apreq_escape(char *dest, const char *src, const apr_size_t slen);
apr_ssize_t apreq_unescape(char *dest, const char *src, apr_size_t slen);

/* returns date string, (time_str is offset from "now") formatted
 * either as an NSCOOKIE or HTTP date */
char *apreq_expires(apr_pool_t *p, const char *time_str, const int type);

/* file sizes (KMG) to bytes */
apr_int64_t apreq_atoi64(const char *s);

/* "duration" strings (YMDhms) to seconds */
long apreq_atol(const char *s);

#ifdef __cplusplus
 }
#endif

#endif /* APREQ_H */
