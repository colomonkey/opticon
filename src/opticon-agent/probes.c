#include <sys/utsname.h>
#include <unistd.h>
#include <libopticonf/var.h>
#include "probes.h"

/** List of built-in probe functions */
builtinfunc BUILTINS[] = {
    {"probe_pcpu", runprobe_pcpu},
    {"probe_hostname", runprobe_hostname},
    {"probe_uname", runprobe_uname},
    {NULL, NULL}
};

var *runprobe_pcpu (probe *self) {
    var *res = var_alloc();
    var_set_double_forkey (res, "pcpu", ((double)(rand()%1000))/10.0);
    return res;
}

var *runprobe_hostname (probe *self) {
    char out[256];
    gethostname (out, 255);
    var *res = var_alloc();
    var_set_str_forkey (res, "hostname", out);
    return res;
}

var *runprobe_uname (probe *self) {
    char out[256];
    var *res = var_alloc();
    struct utsname uts;
    uname (&uts);
    sprintf (out, "%s %s %s", uts.sysname,
             uts.release, uts.machine);
    var_set_str_forkey (res, "uname", out);
    return res;
}
