#include <libopticonf/parse.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>

static const char *VALIDUNQUOTED = "abcdefghijklmnopqrstuvwxyz"
                                   "ABCDEFGHIJKLMNOPQRSTUVWXUZ"
                                   "0123456789-_.";

static const char *VALIDUNQUOTEDV = "abcdefghijklmnopqrstuvwxyz"
                                    "ABCDEFGHIJKLMNOPQRSTUVWXUZ"
                                    "0123456789-_./:";

static char LAST_PARSE_ERROR[4096];
static int LAST_PARSE_LINE = 0;

typedef enum parse_state_e {
    PSTATE_DICT_WAITKEY,
    PSTATE_DICT_KEY,
    PSTATE_DICT_KEY_QUOTED,
    PSTATE_DICT_WAITVALUE,
    PSTATE_DICT_VALUE,
    PSTATE_DICT_VALUE_QUOTED,
    PSTATE_ARRAY_WAITVALUE,
    PSTATE_ARRAY_VALUE,
    PSTATE_ARRAY_VALUE_QUOTED,
    PSTATE_COMMENT
} parse_state;

/** Internal state machine for parsing a JSON-like configuration
  * text. Recurses on itself for new levels of hierarchy.
  * \param v The var at this cursor level.
  * \param buf The cursor (inout)
  * \param st The state to start out with.
  */
int parse_config_level (var *v, const char **buf, parse_state st) {
    const char *c = *buf;
    var *vv = NULL;
    char keybuf[4096];
    int keybuf_pos = 0;
    char valuebuf[4096];
    int valuebuf_pos = 0;
    int value_nondigits = 0;
    parse_state stnext;

    while (*c) {    
        switch (st) {
            case PSTATE_DICT_WAITKEY:
                if (*c == '#') {
                    stnext = st;
                    st = PSTATE_COMMENT;
                    break;
                }
                if (isspace (*c)) break;
                if (*c == ',') break;
                if (*c == '}') {
                    *buf = c;
                    return 1;
                }
                if (*c == '\"') st = PSTATE_DICT_KEY_QUOTED;
                else {
                    if (! strchr (VALIDUNQUOTED, *c)) {
                        sprintf (LAST_PARSE_ERROR, "Invalid char for "
                                 "key: '%c'", *c);
                        return 0;
                    }
                    --c;
                    st = PSTATE_DICT_KEY;
                }
                keybuf_pos = 0;
                keybuf[0] = 0;
                valuebuf_pos = 0;
                valuebuf[0] = 0;
                break;
        
            case PSTATE_DICT_KEY:
                if (*c == '#') {
                    stnext = PSTATE_DICT_WAITVALUE;
                    st = PSTATE_COMMENT;
                    break;
                }
                if (isspace (*c)) {
                    st = PSTATE_DICT_WAITVALUE;
                    break;
                }
                if (*c == '{') {
                    *buf = c+1;
                    vv = var_get_dict_forkey (v, keybuf);
                    if (! vv) {
                        sprintf (LAST_PARSE_ERROR, "Couldn't get dict "
                                 "for key '%s'", keybuf);
                        return 0;
                    }
                    if (!parse_config_level (vv, buf, PSTATE_DICT_WAITKEY)) {
                        return 0;
                    }
                    c = *buf;
                    st = PSTATE_DICT_WAITKEY;
                    break;
                }
                if (*c == '[') {
                    *buf = c+1;
                    vv = var_get_array_forkey (v, keybuf);
                    if (! vv) {
                        sprintf (LAST_PARSE_ERROR, "Couldn't get array "
                                 "for key '%s'", keybuf);
                        return 0;
                    }
                    var_clear_array (vv);
                    if (!parse_config_level (vv, buf, 
                                             PSTATE_ARRAY_WAITVALUE)) {
                        return 0;
                    }
                    c = *buf;
                    st = PSTATE_DICT_WAITKEY;
                    break;
                }
                if (*c == ':') {
                    st = PSTATE_DICT_WAITVALUE;
                    break;
                }
                if (! strchr (VALIDUNQUOTED, *c)) {
                    sprintf (LAST_PARSE_ERROR, "Invalid character in "
                             "value '%c'", *c);
                    return 0;
                }
                if (keybuf_pos >= 4095) return 0;
                keybuf[keybuf_pos++] = *c;
                keybuf[keybuf_pos] = 0;
                break;
            
            case PSTATE_DICT_KEY_QUOTED:
                if (*c == '\"') {
                    st = PSTATE_DICT_WAITVALUE;
                    break;
                }
                if (*c == '\\') ++c;
                if (keybuf_pos >= 4095) return 0;
                keybuf[keybuf_pos++] = *c;
                keybuf[keybuf_pos] = 0;
                break;
            
            case PSTATE_DICT_WAITVALUE:
                if (*c == '#') {
                    stnext = st;
                    st = PSTATE_COMMENT;
                    break;
                }
                if (isspace (*c)) break;
                if (*c == ':') break;
                if (*c == '=') break;
                if (*c == '{') {
                    *buf = c+1;
                    vv = var_get_dict_forkey (v, keybuf);
                    if (! vv) {
                        sprintf (LAST_PARSE_ERROR, "Couldn't get dict for "
                                 "key '%s'", keybuf);
                        return 0;
                    }
                    if (!parse_config_level (vv, buf, PSTATE_DICT_WAITKEY)) {
                        return 0;
                    }
                    c = *buf;
                    st = PSTATE_DICT_WAITKEY;
                    break;
                }
                if (*c == '[') {
                    *buf = c+1;
                    vv = var_get_array_forkey (v, keybuf);
                    if (! vv) {
                        sprintf (LAST_PARSE_ERROR, "Couldn't get array "
                                 "for key '%s'", keybuf);
                        return 0;
                    }
                    var_clear_array (vv);
                    if (!parse_config_level (vv, buf, 
                                             PSTATE_ARRAY_WAITVALUE)) {
                        return 0;
                    }
                    c = *buf;
                    st = PSTATE_DICT_WAITKEY;
                    break;
                }
                if (*c == '\"') {
                    st = PSTATE_DICT_VALUE_QUOTED;
                }
                else {
                    if (! strchr (VALIDUNQUOTED, *c)) {
                        sprintf (LAST_PARSE_ERROR, "Invalid character for "
                                 "value: '%c'", *c);
                        return 0;
                    }
                    --c;
                    value_nondigits = 0;
                    st = PSTATE_DICT_VALUE;
                }
                valuebuf_pos = 0;
                valuebuf[0] = 0;
                break; 
            
            case PSTATE_DICT_VALUE:
                if (isspace (*c) || (*c == ',') || (*c == '}') ||
                    (*c == '#')) {
                    if (! value_nondigits) {
                        var_set_int_forkey (v, keybuf, atoi(valuebuf));
                    }
                    else var_set_str_forkey (v, keybuf, valuebuf);
                    if (*c == '#') {
                        stnext = PSTATE_DICT_WAITKEY;
                        st = PSTATE_COMMENT;
                        break;
                    }
                    if (*c == '}') {
                        *buf = c;
                        return 1;
                    }
                    st = PSTATE_DICT_WAITKEY;
                    break;
                }
                if (! strchr (VALIDUNQUOTEDV, *c)) {
                    sprintf (LAST_PARSE_ERROR, "Invalid character in "
                             "value: '%c'", *c);
                    return 0;
                }
                if ((!value_nondigits) && (*c<'0' || *c>'9')) {
                    value_nondigits = 1;
                }
                if (valuebuf_pos >= 4095) return 0;
                valuebuf[valuebuf_pos++] = *c;
                valuebuf[valuebuf_pos] = 0;
                break;
            
            case PSTATE_DICT_VALUE_QUOTED:
                if (*c == '\"') {
                    if (atoi (valuebuf) || valuebuf[0] == '0') {
                        var_set_int_forkey (v, keybuf, atoi(valuebuf));
                    }
                    else var_set_str_forkey (v, keybuf, valuebuf);
                    st = PSTATE_DICT_WAITKEY;
                    break;
                }
                if (*c == '\\') ++c;
                if (valuebuf_pos >= 4095) return 0;
                valuebuf[valuebuf_pos++] = *c;
                valuebuf[valuebuf_pos] = 0;
                break;
                
            case PSTATE_ARRAY_WAITVALUE:
                if (*c == '#') {
                    stnext = st;
                    st = PSTATE_COMMENT;
                    break;
                }
                if (isspace (*c)) break;
                if (*c == ',') break;
                if (*c == ']') {
                    *buf = c;
                    return 1;
                }
                if (*c == '\"') {
                    st = PSTATE_ARRAY_VALUE_QUOTED;
                }
                else {
                    if (! strchr (VALIDUNQUOTED, *c)) return 0;
                    --c;
                    value_nondigits = 0;
                    st = PSTATE_ARRAY_VALUE;
                }
                valuebuf_pos = 0;
                valuebuf[0] = 0;
                break;
            
            case PSTATE_ARRAY_VALUE:
                if (isspace (*c) || (*c == ']') || 
                    (*c == ',') || (*c == '#')) {
                    if (! value_nondigits) {
                        var_add_int (v, atoi (valuebuf));
                    }
                    else var_add_str (v, valuebuf);
                    if (*c == '#') {
                        stnext = PSTATE_ARRAY_WAITVALUE;
                        st = PSTATE_COMMENT;
                        break;
                    }
                    if (*c == '}') {
                        *buf = c;
                        return 1;
                    }
                    st = PSTATE_ARRAY_WAITVALUE;
                    break;
                }
                if (! strchr (VALIDUNQUOTEDV, *c)) return 0;
                if ((!value_nondigits) && (*c<'0' || *c>'9')) {
                    value_nondigits = 1;
                }
                if (valuebuf_pos >= 4095) return 0;
                valuebuf[valuebuf_pos++] = *c;
                valuebuf[valuebuf_pos] = 0;
                break;
            
            case PSTATE_ARRAY_VALUE_QUOTED:
                if (*c == '\"') {
                    var_add_str (v, valuebuf);
                    st = PSTATE_ARRAY_WAITVALUE;
                    break;
                }
                if (*c == '\\') ++c;
                if (valuebuf_pos >= 4095) return 0;
                valuebuf[valuebuf_pos++] = *c;
                valuebuf[valuebuf_pos] = 0;
                break;
            
            case PSTATE_COMMENT:
                if (*c == '\n') {
                    st = stnext;
                }
                break;
        }
        ++c;
        *buf = c;
    }
    return 1;
}

/** Parse a configuration text into a variable space.
  * \param into The root of the variable space.
  * \param buf The configuration text.
  * \return 1 on success, 0 on failure.
  */
int parse_config (var *into, const char *buf) {
    const char *crsr = buf;
    int res = parse_config_level (into, &crsr, PSTATE_DICT_WAITKEY);
    if (! res) {
        char errbuf[64];
        LAST_PARSE_LINE=1;
        const char *c = buf;
        while (c < crsr) {
            if (*c == '\n') LAST_PARSE_LINE++;
            c++;
        }
        snprintf (errbuf, 63, " (line %i)", LAST_PARSE_LINE);
        errbuf[63] = 0;
        strncat (LAST_PARSE_ERROR, errbuf, 4095);
        LAST_PARSE_ERROR[4095] = 0;
    }
    return res;
}

/** Return a text string detailing the last parse error encountered. */
const char *parse_error (void) {
    return LAST_PARSE_ERROR;
}