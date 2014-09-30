#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include <libopticon/util.h>
#include <libopticon/parse.h>
#include "req_context.h"

/** Allocate a request argument list */
req_arg *req_arg_alloc (void) {
    req_arg *self = (req_arg *) malloc (sizeof (req_arg));
    self->argc = 0;
    self->argv = NULL;
    return self;
}

/** Add a preallocated string to an argument list.
  * \param self The list
  * \param a The string to add
  */
void req_arg_add_nocopy (req_arg *self, const char *a) {
    if (self->argc) {
        self->argv = (char **) realloc (self->argv,
                                 sizeof (req_arg) * (self->argc+1));
    }
    else self->argv = (char **) malloc (sizeof (req_arg));
    self->argv[self->argc] = (char *) a;
    self->argc++;
}

/** Allocate and copy a string, adding it to an argument list.
  * \param self The list
  * \param a The string to add
  */
void req_arg_add (req_arg *self, const char *a) {
    req_arg_add_nocopy (self, strdup (a));
}

/** Clear an argument list */
void req_arg_clear (req_arg *self) {
    for (int i=0; i<self->argc; ++i) free (self->argv[i]);
    self->argc = 0;
    self->argv = NULL;
}

/** Clear and deallocate an argument list */
void req_arg_free (req_arg *self) {
    req_arg_clear (self);
    free (self);
}

/** Initialize a matchlist header */
void req_matchlist_init (req_matchlist *self) {
    self->first = self->last = NULL;
}

/** Add a match definition to a matchlist.
  * \param self The list
  * \param s The match definition
  * \param mmask The mask of methods caught
  * \param f The function to call
  */
void req_matchlist_add (req_matchlist *self, const char *s,
                        req_method mmask, path_f f) {
    req_match *m = (req_match *) malloc (sizeof (req_match));
    m->next = m->prev = NULL;
    m->matchstr = s;
    m->method_mask = mmask;
    m->func = f;
    
    if (self->first) {
        m->prev = self->last;
        self->last->next = m;
        self->last = m;
    }
    else {
        self->first = self->last = m;
    }
}

/** Check a url against a req_match definition.
  * \param self The match definition
  * \param url The url string
  * \param arg Caught arguments are stored here.
  * \return 1 On match, 0 on fail.
  */
int req_match_check (req_match *self, const char *url, req_arg *arg) {
    req_arg_clear (arg);
    
    printf ("match %s\n", self->matchstr);
    
    const char *curl = url;
    const char *curl_start = url;
    const char *cmatch = self->matchstr;
    while (*curl) {
        printf ("matchround '%c' '%c'\n", *curl, *cmatch);
        /* complete wildcard, always matches */
        if (*cmatch == '*' && cmatch[1] == 0) return 1;
        /* '*' wildcard, just skip over to the next matching char */
        if (*cmatch == '*') {
            while (*curl && *curl != cmatch[1]) curl++;
            if (! *curl) {
                /* end of url, end of match? */
                if (cmatch[1]) return 0;
                return 1;
            }
            cmatch++;
        }
        /* '%' argument, add argument */
        else if (*cmatch == '%') {
            char matchtp = cmatch[1];
            int validch = 0;
            assert (strchr ("sUT", matchtp));
            curl_start = curl;
            cmatch++;
            
            /* Skip over the url until the next slash or the end */
            while (*curl && *curl != '/') {
                /* Make sure we're getting valid data for the type */
                switch (matchtp) {
                    case 's': break;
                    case 'U':
                        if (!strchr ("0123456789abcdef-", *curl)) {
                            return 0;
                        }
                        if (*curl != '-') validch++;
                        break;
                    
                    case 'T':
                        if (!strchr ("0123456789-:TZ", *curl)) {
                            return 0;
                        }
                        if (*curl>='0' && *curl<='9') validch++;
                        break;
                }
                curl++;
            }
            
            /* Reject invalid UUID / timespec */
            if (matchtp == 'U' && validch != 32) return 0;
            if (matchtp == 'T' && validch != 12) return 0;
            
            /* Copy the url-part to the argument list */
            size_t elmsize = curl-curl_start;
            if (! elmsize) return 0;
            char *elm = (char *) malloc (elmsize+1);
            memcpy (elm, curl_start, elmsize);
            elm[elmsize] = 0;
            req_arg_add_nocopy (arg, elm); /*req_arg will free() elm*/
        }
        if (*cmatch != *curl) return 0;
        if (! *curl) return 1;
        curl++;
        cmatch++;
    }
    return 1;
}

/** Path handler for a generic server error (500) */
int err_server_error (req_context *ctx, struct MHD_Connection *conn,
                      req_arg *arg) {
    const char *buf = "{\"error\":\"Internal Server Error\"}";
    struct MHD_Response *response;
    response = MHD_create_response_from_data (strlen (buf), (void*) buf, 1, 1);
    MHD_queue_response (conn, 500, response);
    MHD_destroy_response (response);
    return 1;    
}

/** Take a request context and connection and shop it around inside
  * a request matchlist.
  * \param self The matchlist
  * \param url The URL string
  * \param ctx The request context
  * \param connection The microhttp connection */
void req_matchlist_dispatch (req_matchlist *self, const char *url,
                             req_context *ctx,
                             struct MHD_Connection *connection) {
                             
    req_arg *targ = req_arg_alloc();
    printf ("dispatch url: %s\n", url);

    /* Iterate over the list */
    req_match *crsr = self->first;
    while (crsr) {
        printf ("ctxmethod %i methodmask %i\n", ctx->method, crsr->method_mask);
        if (ctx->method & crsr->method_mask) {
            if (req_match_check (crsr, url, targ)) {
                printf ("calling\n");
                if (crsr->func (ctx, connection, targ)) {
                    printf ("buck stops\n");
                    req_arg_free (targ);
                    return;
                }
            }
        }
        crsr = crsr->next;
    }

    /* No matches, bail out */    
    err_server_error (ctx, connection, targ);
    req_arg_free (targ);
}

/** Allocate a request context object */
req_context *req_context_alloc (void) {
    req_context *self = (req_context *) malloc (sizeof (req_context));
    if (self) {
        self->headers = var_alloc();
        self->bodyjson = var_alloc();
        self->response = var_alloc();
        self->auth_data = var_alloc();
        self->status = 500;
        self->url = NULL;
        self->ctype = NULL;
        self->body = NULL;
        self->bodysz = self->bodyalloc = 0;
        memset (&self->tenantid, 0, sizeof (uuid));
        memset (&self->hostid, 0, sizeof (uuid));
        memset (&self->opticon_token, 0, sizeof (uuid));
        self->openstack_token = NULL;
    }
    return self;
}

/** Deallocate a request context object and all its child structures */
void req_context_free (req_context *self) {
    var_free (self->headers);
    var_free (self->bodyjson);
    var_free (self->response);
    var_free (self->auth_data);
    if (self->url) {
        free (self->url);
    }
    if (self->body) free (self->body);
    if (self->openstack_token) free (self->openstack_token);
    free (self);
}

/** Set a header inside the request context */
void req_context_set_header (req_context *self, const char *hdrname,
                             const char *hdrval) {
    char *canon = strdup (hdrname);
    char *c = canon;
    int needcap=1;
    while (*c) {
        if (needcap) { *c = toupper(*c); needcap = 0; }
        else if (*c == '-') needcap = 1;
        else *c = tolower (*c);
        c++;
    }
    var_set_str_forkey (self->headers, canon, hdrval);
    
    /** Handle Openstack Token */
    if (strcmp (canon, "X-Auth-Token") == 0) {
        self->openstack_token = strdup (hdrval);
    }
    else if (strcmp (canon, "X-Opticon-Token") == 0) {
        self->opticon_token = mkuuid (hdrval);
    }
    else if (strcmp (canon, "Content-Type") == 0) {
        self->ctype = strdup (hdrval);
    }
    
    free (canon);
}

/** Parse the loaded body data inside the request context as JSON */
int req_context_parse_body (req_context *self) {
    if (! self->body) return 1;
    if (strcasecmp (self->ctype, "application/json") != 0) return 0;
    return parse_json (self->bodyjson, self->body);
}

/** Internal function for sizing the body data buffer */
static size_t mkallocsz (size_t target) {
    if (target < 4096) return 4096;
    if (target < 8192) return 8192;
    if (target < 16384) return 16384;
    if (target < 65536) return 65536;
    return target + 4096;
}

/** Handle uploaded data */
void req_context_append_body (req_context *self, const char *dt, size_t sz) {
    if (! self->body) {
        self->bodyalloc = mkallocsz (sz);
        self->body = (char *) malloc (self->bodyalloc);
        self->bodysz = 0;
    }
    if ((self->bodysz + sz + 1) > self->bodyalloc) {
        self->bodyalloc = mkallocsz (self->bodysz + sz + 1);
        self->body = (char *) realloc (self->body, self->bodyalloc);
    }
    memcpy (self->body + self->bodysz, dt, sz);
    self->bodysz += sz;
    self->body[self->bodysz] = 0;
}

/** Set the url */
void req_context_set_url (req_context *self, const char *url) {
    if (self->url) free (self->url);
    self->url = strdup (url);
}

/** Set the request method */
void req_context_set_method (req_context *self, const char *mth) {
    req_method m = REQ_OTHER;
    if (strcasecmp (mth, "get") == 0) m = REQ_GET;
    else if (strcasecmp (mth, "post") == 0) m = REQ_POST;
    else if (strcasecmp (mth, "put") == 0) m = REQ_PUT;
    else if (strcasecmp (mth, "delete") == 0) m = REQ_DELETE;
    self->method = m;
}