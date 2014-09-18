#include <libopticonf/var.h>
#include <libopticonf/parse.h>
#include <libopticon/datatypes.h>
#include <libopticon/util.h>

int import_dictlevel (host *into, const char *prefix, var *v) {
    char idstr[32];
    strcpy (idstr, prefix);
    var *firstnode = v->value.arr.first;
    var *nodecrsr = firstnode;
    int count = var_get_count (v->parent);
    const char *childid = NULL;
    meterid_t mid;
    meter *m;
    const char *ss;
    int ii;
    double dd;
    
    var *vcrsr = firstnode;
    while (vcrsr) {
        childid = vcrsr->id;
        if (strlen (prefix) + strlen (childid) > 10) {
            fprintf (stderr, "Meter name too long: %s/%s",
                     prefix, childid);
            vcrsr = vcrsr->next;
            continue;
        }
        sprintf (idstr, "%s/%s", prefix, childid);
        switch (vcrsr->type) {
            case VAR_NULL:
            case VAR_DICT:
            case VAR_ARRAY:
                fprintf (stderr, "Type not supported for %s\n", idstr);
                vcrsr = vcrsr->next;
                continue;
            
            case VAR_INT:
                mid = makeid (idstr, MTYPE_INT, 0);
                m = host_get_meter (into, mid);
                meter_setcount (m,count);
                for (int i=0; i<count; ++i) {
                    nodecrsr = var_get_dict_atindex (v->parent, i);
                    ii = var_get_int_forkey (nodecrsr, childid);
                    if (ii<0) continue;
                    meter_set_uint (m, i, ii);
                }
                break;
                
            case VAR_DOUBLE:
                mid = makeid (idstr, MTYPE_FRAC, 0);
                m = host_get_meter (into, mid);
                meter_setcount (m,count);
                for (int i=0; i<count; ++i) {
                    nodecrsr = var_get_dict_atindex (v->parent, i);
                    dd = var_get_double_forkey (nodecrsr, childid);
                    if (dd<0.0 || dd>255.0) continue;
                    meter_set_frac (m, i, dd);
                }
                break;
            
            case VAR_STR:
                mid = makeid (idstr, MTYPE_STR, 0);
                m = host_get_meter (into, mid);
                meter_setcount (m,count);
                for (int i=0; i<count; ++i) {
                    nodecrsr = var_get_dict_atindex (v->parent, i);
                    ss = var_get_str_forkey (nodecrsr, childid);
                    meter_set_str (m, i, ss);
                }
                break;
            
            default:
                break;
        }
        vcrsr = vcrsr->next;
    }

    return 1;
}

void dobreak (void) {}

int import_json (host *into, const char *json) {
    var *dat = var_alloc();
    char idstr[32];
    
    if (! parse_json (dat, json)) {
        fprintf (stderr, "Parse error: %s\n", parse_error());
        var_free (dat);
        return 0;
    }
    
    dobreak();
    
    var *crsr = dat->value.arr.first;
    var *cc;
    meter *m = NULL;
    meterid_t mid;
    vartype typ;
    int ival;
    double dval;
    int toplen;
    
    while (crsr) {
        if (strlen (crsr->id) > 11) {
            fprintf (stderr, "Meter name too long: %s\n", crsr->id);
            var_free (dat);
            return 0;
        }
        
        switch (crsr->type) {
            case VAR_NULL:
                fprintf (stderr, "Warning: skipping %s\n", crsr->id);
                break;
            
            case VAR_INT:
                ival = crsr->value.ival;
                if (ival < 0) {
                    fprintf (stderr, "Warning: cannot encode negative "
                             "value for %s\n", crsr->id);
                    ival = 0;
                }
                mid = makeid (crsr->id, MTYPE_INT, 0);
                m = host_get_meter (into, mid);
                meter_setcount (m, 0);
                meter_set_uint (m, 0, ival);
                break;
            
            case VAR_DOUBLE:
                dval = crsr->value.dval;
                if ((dval<0.0)||(dval>255.0)) {
                    fprintf (stderr, "Warning: value %.3f out of "
                             "range", dval);
                    if (dval<0.0) dval = 0.0;
                    else dval = 255.0;
                }
                mid = makeid (crsr->id, MTYPE_FRAC, 0);
                m = host_get_meter (into, mid);
                meter_setcount (m, 0);
                meter_set_frac (m, 0, dval);
                break;
            
            case VAR_STR:
                mid = makeid (crsr->id, MTYPE_STR, 0);
                m = host_get_meter (into, mid);
                meter_setcount (m, 0);
                meter_set_str (m, 0, crsr->value.sval);
                break;
            
            case VAR_ARRAY:
                cc = crsr->value.arr.first;
                if (! cc) continue;
                typ = cc->type;
                while (cc) {
                    if (cc->type != typ) {
                        fprintf (stderr, "Array %s not uniform\n", crsr->id);
                        var_free (dat);
                        return 0;
                    }
                    cc = cc->next;
                }
                cc = crsr->value.arr.first;
                if (typ == VAR_DICT) {
                    import_dictlevel (into, crsr->id, cc);
                    break;
                }
                if (typ == VAR_ARRAY) {
                    fprintf (stderr, "Nested arrays not supported\n");
                    var_free (dat);
                    return 0;
                }
                switch (typ) {
                    case VAR_NULL:
                    case VAR_INT:
                        mid = makeid (crsr->id, MTYPE_INT, 0);
                        break;
                    case VAR_DOUBLE:
                        mid = makeid (crsr->id, MTYPE_FRAC, 0);
                        break;
                    case VAR_STR:
                        mid = makeid (crsr->id, MTYPE_STR, 0);
                        break;
                    default:
                        break;
                }
                
                m = host_get_meter (into, mid);
                meter_setcount (m, crsr->value.arr.count);
                for (int i=0; i< crsr->value.arr.count; ++i) {
                    cc = var_find_index (crsr, i);
                    if (! cc) continue;
                    switch (typ) {
                        case VAR_NULL:
                        case VAR_INT:
                            if (cc->value.ival <0) break;
                            meter_set_uint (m, i, cc->value.ival);
                            break;
                        case VAR_DOUBLE:
                            if (cc->value.dval < 0.0) break;
                            if (cc->value.dval > 255.0) break;
                            meter_set_frac (m, i, cc->value.dval);
                            break;
                        case VAR_STR:
                            meter_set_str (m, i, cc->value.sval);
                            break;
                        default:
                            break;
                    }
                }
            
            case VAR_DICT:
                cc = crsr->value.arr.first;
                toplen = strlen (crsr->id);
                while (cc) {
                    if (strlen (cc->id) + toplen > 10) {
                        fprintf (stderr, "Meter name too long: %s/%s",
                                 crsr->id, cc->id);
                        continue;
                    }
                    strcpy (idstr, crsr->id);
                    strcat (idstr, "/");
                    strcat (idstr, cc->id);
                    switch (cc->type) {
                        case VAR_INT:
                            if (cc->value.ival<0) break;
                            mid = makeid (idstr, MTYPE_INT, 0);
                            m = host_get_meter (into, mid);
                            meter_setcount (m, 0);
                            meter_set_uint (m, 0, cc->value.ival);
                            break;
                        
                        case VAR_DOUBLE:
                            if (cc->value.dval<0.0) break;
                            if (cc->value.dval>255.0) break;
                            mid = makeid (idstr, MTYPE_FRAC, 0);
                            m = host_get_meter (into, mid);
                            meter_setcount (m, 0);
                            meter_set_frac (m, 0, cc->value.dval);
                            break;
                        
                        case VAR_STR:
                            mid = makeid (idstr, MTYPE_STR, 0);
                            m = host_get_meter (into, mid);
                            meter_setcount (m, 0);
                            meter_set_str (m, 0, cc->value.sval);
                            break;
                        
                        case VAR_ARRAY:
                            fprintf (stderr, "Cannot nest array in dict\n");
                            cc = cc->next;
                            continue;
                        
                        case VAR_DICT:
                            fprintf (stderr, "Cannot nest dicts\n");
                            cc = cc->next;
                            continue;
                            
                        default:
                            break;
                    }
                    cc = cc->next;
                }
                break;
                
            default:
                break;
        }
        crsr = crsr->next;
    }
    return 1;
}
