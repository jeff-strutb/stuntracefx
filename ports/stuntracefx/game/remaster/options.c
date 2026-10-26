/*
 * options.c: the Remaster Options (rm_options.h), kept in the console's
 * save folder.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "console_ext.h"
#include "rm_options.h"

static const struct { const char *name; int def; } k_opt[RM_OPT_COUNT] = {
    [RM_OPT_FRAMERATE] = { "framerate", 0 },   /* 0 the original's, 1 60 a second */
    [RM_OPT_WIDESCREEN] = { "widescreen", 1 }, /* 1 a race fills the window, 0 the original's 4:3 */
    [RM_OPT_AUTOACCEL] = { "autoaccelerate", 1 }, /* touch: 1 the gas held while driving, 0 the wheel's finger holds it */
};
static int value[RM_OPT_COUNT], forced[RM_OPT_COUNT], loaded;

static void cfg_path(char *out, size_t n)
{
    snprintf(out, n, "%s/remaster.cfg", nshvc_save_dir());
}

void rm_options_load(void)
{
    char path[1024], line[128], name[64], env[64];
    FILE *f;
    int i, v;

    loaded = 1;
    for (i = 0; i < RM_OPT_COUNT; i++)
        value[i] = k_opt[i].def;
    cfg_path(path, sizeof path);
    if ((f = fopen(path, "r"))) {
        while (fgets(line, sizeof line, f))
            if (sscanf(line, "%63s %d", name, &v) == 2)
                for (i = 0; i < RM_OPT_COUNT; i++)
                    if (!strcmp(name, k_opt[i].name))
                        value[i] = v;
        fclose(f);
    }
    for (i = 0; i < RM_OPT_COUNT; i++) {
        const char *e;
        size_t k;
        snprintf(env, sizeof env, "RM_OPT_%s", k_opt[i].name);
        for (k = 7; env[k]; k++)
            if (env[k] >= 'a' && env[k] <= 'z')
                env[k] = (char)(env[k] - 32);
        if ((e = getenv(env))) {
            value[i] = atoi(e);
            forced[i] = 1;
        }
    }
}

int rm_option(int which)
{
    if (!loaded)
        rm_options_load();
    return value[which];
}

void rm_option_set(int which, int v)
{
    char path[1024];
    FILE *f;
    int i;

    if (!loaded)
        rm_options_load();
    value[which] = v;
    forced[which] = 0;
    cfg_path(path, sizeof path);
    if (!(f = fopen(path, "w")))
        return;
    for (i = 0; i < RM_OPT_COUNT; i++)
        if (!forced[i])
            fprintf(f, "%s %d\n", k_opt[i].name, value[i]);
    fclose(f);
}
