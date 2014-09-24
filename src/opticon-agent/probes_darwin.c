#include <libsvc/log.h>
#include <ctype.h>

typedef struct toprec_s {
    uint32_t pid;
    char cmd[32];
    double pcpu;
    uint32_t sizekb;
    char user[32];
} toprec;

typedef struct topinfo_s {
    toprec records[16];
    double cpu_usage;
    uint32_t memsizekb;
    double load1;
    double load5;
    double load15;
} topinfo;

topinfo TOP[2];

typedef enum { TOP_HDR, TOP_BODY } topstate;

static void cpystr (char *into, const char *ptr, int sz) {
    int eos = 0;
    for (int i=0; i<sz; ++i) {
        if (ptr[i] == '\n') break;
        into[i] = ptr[i];
        if (into[i] != ' ') eos = i+1;
    }
    into[eos] = 0;
}

static uint32_t sz2kb (const char *str) {
    int ival = atoi (str);
    if (! ival) return ival;
    
    const char *c = str;
    while (isdigit (*c)) c++;
    if (*c == 'B') ival /= 1024;
    else if (*c == 'M') ival *= 1024;
    else if (*c == 'G') ival *= (1024*1024);
    return ival;
}

void run_top (thread *me) {
    char buf[1024];
    topstate st = TOP_HDR;
    int count = 0;
    int offs_cmd, offs_cpu, offs_mem, offs_user;
    memset (TOP, 0, 2*sizeof(topinfo));
    FILE *f = popen ("/usr/bin/top -l 0 -o cpu -s 20", "r");
    if (! f) {
        log_error ("Could not open top probe");
        return;
    }
    while (1) {
        if (feof (f)) {
            pclose (f);
            f = popen ("/usr/bin/top -l 0 -o cpu -s 10", "r");
        }
        fgets (buf, 1023, f);
        
        if (buf[0] == 'P' && buf[1] == 'I' && buf[2] == 'D') {
            st = TOP_BODY;
            memset (TOP[1].records, 0, 16 *sizeof (toprec));
            offs_cmd = strstr (buf, "COMMAND ") - buf;
            offs_cpu = strstr (buf, "%CPU ") - buf;
            offs_mem = strstr (buf, "MEM ") -buf;
            offs_user = strstr (buf, "USER ") - buf;
            continue;
        }
        if (st == TOP_BODY) {
            if (strlen (buf) < 240) continue;
            if (atof (buf+offs_cpu) < 0.0001) {
                memcpy (TOP, TOP+1, sizeof (topinfo));
                memset (TOP+1, 0, sizeof(topinfo));
                st = TOP_HDR;
                count = 0;
                continue;
            }
            TOP[1].records[count].pid = atoi (buf);
            cpystr (TOP[1].records[count].cmd, buf+offs_cmd, 15);
            TOP[1].records[count].pcpu = atof (buf+offs_cpu);
            TOP[1].records[count].sizekb = sz2kb (buf+offs_mem);
            cpystr (TOP[1].records[count].user, buf+offs_user, 15);
            count++;
            if (count == 14) {
                memcpy (TOP, TOP+1, sizeof (topinfo));
                memset (TOP+1, 0, sizeof(topinfo));
                st = TOP_HDR;
                count = 0;
            }
        }
        else {
            if (memcmp (buf, "Load Avg", 8) == 0) {
                TOP[1].load1 = atof (buf + 10);
                char *next = strchr (buf, ',');
                if (next) {
                    TOP[1].load5 = atof (next+2);
                    next = strchr (next+2, ',');
                    if (next) {
                        TOP[1].load15 = atof (next+2);
                    }
                }
            }
            else if (memcmp (buf, "CPU usage: ", 11) == 0) {
                TOP[1].cpu_usage = atof (buf+11);
                char *next = strchr (buf, ',');
                if (next) TOP[1].cpu_usage += atof (next+2);
            }
            else if (memcmp (buf, "VM: ", 3) == 0) {
                TOP[1].memsizekb = sz2kb (buf+4);
            }
        }
    }
}

thread *TOPTHREAD = NULL;

var *runprobe_top (probe *self) {
    if (! TOPTHREAD) {
        TOPTHREAD = thread_create (run_top, NULL);
        sleep (3);
    }
    
    topinfo ti;
    memcpy (&ti, TOP, sizeof (topinfo));
    
    var *res = var_alloc();
    var *res_top = var_get_array_forkey (res, "top");
    for (int i=0; ti.records[i].cmd[0] && i<15; ++i) {
        var *procrow = var_add_dict (res_top);
        var_set_int_forkey (procrow, "pid", ti.records[i].pid);
        var_set_str_forkey (procrow, "user", ti.records[i].user);
        var_set_str_forkey (procrow, "name", ti.records[i].cmd);
        var_set_double_forkey (procrow, "pcpu", ti.records[i].pcpu);
        double memperc = (double) ti.records[i].sizekb;
        memperc = memperc / (double) ti.memsizekb;
        memperc = memperc * 100.0;
        var_set_double_forkey (procrow, "pmem", memperc);
    }
    var_set_double_forkey (res, "pcpu", ti.cpu_usage);
    var *arr = var_get_array_forkey (res, "loadavg");
    var_add_double (arr, ti.load1);
    var_add_double (arr, ti.load5);
    var_add_double (arr, ti.load15);
    FILE *out = fopen ("toprec.json","w");
    dump_var (res, out);
    fclose (out);
    return res;
}

var *runprobe_df (probe *self) {
    char buffer[1024];
    char device[32];
    char mount[32];
    uint64_t szkb;
    double pused;
    char *crsr;
    var *res = var_alloc();
    var *v_df = var_get_array_forkey (res, "df");
    FILE *f = popen ("/bin/df -k", "r");
    while (! feof (f)) {
        fgets (buffer, 1023, f);
        if (memcmp (buffer, "/dev", 4) != 0) continue;
        cpystr (device, buffer, 12);
        crsr = buffer;
        while ((*crsr) && (! isspace (*crsr))) crsr++;
        if (! *crsr) continue;
        while ((*crsr) && (isspace (*crsr))) crsr++;
        if (! *crsr) continue;
        szkb = strtoull (crsr, &crsr, 10);

        /* skip to used */ 
        while ((*crsr) && (! isspace (*crsr))) crsr++;
        if (! *crsr) continue;
        while ((*crsr) && (isspace (*crsr))) crsr++;
        if (! *crsr) continue;
       
        /* skip to available */ 
        while ((*crsr) && (! isspace (*crsr))) crsr++;
        if (! *crsr) continue;
        while ((*crsr) && (isspace (*crsr))) crsr++;
        if (! *crsr) continue;

        /* skip to used% */ 
        while ((*crsr) && (! isspace (*crsr))) crsr++;
        if (! *crsr) continue;
        while ((*crsr) && (isspace (*crsr))) crsr++;
        if (! *crsr) continue;
        
        pused = atof (crsr);

        /* skip to iused */ 
        while ((*crsr) && (! isspace (*crsr))) crsr++;
        if (! *crsr) continue;
        while ((*crsr) && (isspace (*crsr))) crsr++;
        if (! *crsr) continue;

        /* skip to ifree */ 
        while ((*crsr) && (! isspace (*crsr))) crsr++;
        if (! *crsr) continue;
        while ((*crsr) && (isspace (*crsr))) crsr++;
        if (! *crsr) continue;

        /* skip to iused% */ 
        while ((*crsr) && (! isspace (*crsr))) crsr++;
        if (! *crsr) continue;
        while ((*crsr) && (isspace (*crsr))) crsr++;
        if (! *crsr) continue;

        /* skip to iused% */ 
        while ((*crsr) && (! isspace (*crsr))) crsr++;
        if (! *crsr) continue;
        while ((*crsr) && (isspace (*crsr))) crsr++;
        if (! *crsr) continue;
        
        cpystr (mount, crsr, 24);
        var *node = var_add_dict (v_df);
        var_set_str_forkey (node, "device", device);
        var_set_int_forkey (node, "size", (uint32_t) (szkb/1024));
        var_set_double_forkey (node, "pused", pused);
        var_set_str_forkey (node, "mount", mount);
    }
    pclose (f);
    return res;
}
