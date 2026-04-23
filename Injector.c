#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <string.h>
#include <limits.h>

static int run(const char* cmd) {
    int r = system(cmd);
    if (r != 0)
        fprintf(stderr, "[ deltoid warn ] command failed (exit %d): %s\n", r, cmd);
    return r;
}

int main(void) {
    struct passwd* pw = getpwuid(getuid());
    if (!pw) {
        fprintf(stderr, "[ deltoid error ] could not resolve home directory\n");
        return 1;
    }
    const char* home = pw->pw_dir;

    // reject home paths with single quotes (would break shell commands)
    if (strchr(home, '\'')) {
        fprintf(stderr, "[ deltoid error ] home path contains illegal character\n");
        return 1;
    }

    printf("\033[1;36m[ deltoid ]\033[0m initializing...\n");

    // reset any existing overrides
    run("flatpak override --user --reset org.vinegarhq.Sober");

    // grant read-only access to just the .so — not the entire home
    char* cmd = malloc(PATH_MAX + 256);
    if (!cmd) return 1;

    snprintf(cmd, PATH_MAX + 256,
        "flatpak override --user --filesystem='%s/libdeltoid.so':ro org.vinegarhq.Sober",
        home);
    run(cmd);

    // set LD_PRELOAD hook
    snprintf(cmd, PATH_MAX + 256,
        "flatpak override --user --env=LD_PRELOAD='%s/libdeltoid.so' org.vinegarhq.Sober",
        home);

    if (run(cmd) == 0)
        printf("\033[1;32m[ deltoid ]\033[0m hook set. launch sober now.\n");
    else
        printf("\033[1;31m[ deltoid ]\033[0m failed to set flatpak override.\n");

    free(cmd);
    return 0;
}
