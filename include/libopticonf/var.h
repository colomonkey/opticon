#ifndef _VAR_H
#define _VAR_H 1

#include <sys/types.h>
#include <stdlib.h>

/* =============================== TYPES =============================== */

struct var_s; /* forward declaration */

/** Lookup structure for var dictionaries and arrays */
typedef struct varlink_s {
    struct var_s    *first; /**< First child */
    struct var_s    *last; /**< Last child */
    uint32_t         count; /**< Number of children */
    int              cachepos; /**< Position of last lookup (-1 = none) */
    struct var_s    *cachenode; /**< Node of last lookup */
} varlink;

/** Supported variable types for var */
typedef enum vartype_e {
    VAR_NULL, /**< Unset value */
    VAR_INT, /**< Integer */
    VAR_STR, /**< Allocated string */
    VAR_DICT, /**< Dictionary */
    VAR_ARRAY /**< Awkward numbered array */
} vartype;

/** Value union for var */
typedef union varvalue_s {
    char            *sval; /**< String value (var owns memory) */
    int              ival; /**< Integer value */
    varlink          arr; /**< Dictionary or array value */
} varvalue;

/** Generic variable storage */
typedef struct var_s {
    struct var_s    *prev; /**< Link to previous node */
    struct var_s    *next; /**< Link to next node */
    struct var_s    *parent; /**< Link to parent node */
    struct var_s    *root; /**< Link to root of variable space */
    char             id[128]; /**< Key, or "" for numbered item */
    vartype          type; /**< Value type */
    varvalue         value; /**< Value */
    uint32_t         generation; /**< Last generation var was seen */
    uint32_t         lastmodified; /**< Last generation var was modified */
    uint32_t         firstseen; /**< First generation var was seen */
} var;

/* ============================= FUNCTIONS ============================= */

var         *var_alloc (void);
void         var_link (var *, var *parent);
void         var_free (var *);

var         *var_find_key (var *, const char *);
int          var_get_count (var *);
int          var_get_int (var *);
const char  *var_get_str (var *);
var         *var_get_dict_forkey (var *, const char *);
var         *var_get_array_forkey (var *, const char *);
int          var_get_int_forkey (var *, const char *);
const char  *var_get_str_forkey (var *, const char *);
var         *var_get_dict_atindex (var *, int);
var         *var_get_array_atindex (var *, int);
int          var_get_int_atindex (var *, int);
const char  *var_get_str_atindex (var *, int);

void         var_new_generation (var *);
void         var_clean_generation (var *);
void         var_set_int (var *, int);
void         var_set_str (var *, const char *);
void         var_set_int_forkey (var *, const char *, int);
void         var_set_str_forkey (var *, const char *, const char *);
void         var_clear_array (var *);
void         var_add_int (var *, int);
void         var_add_str (var *, const char *);
var         *var_add_array (var *);
var         *var_add_dict (var *);

#endif
