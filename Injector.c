#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <string.h>

int main() {
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;
    char cmd[512];

    printf("\033[1;36m[ Deltoid ]\033[0m initializing stealth environment...\n");

    // reset flatpak to clean detection flags
    system("flatpak override --user --reset org.vinegarhq.Sober");

    // grant filesystem access
    snprintf(cmd, sizeof(cmd), "flatpak override --user --filesystem=%s org.vinegarhq.Sober", homedir);
    system(cmd);

    // set injection hook
    snprintf(cmd, sizeof(cmd), "flatpak override --user --env=LD_PRELOAD=%s/libdeltoid.so org.vinegarhq.Sober", homedir);
    
    if (system(cmd) == 0) {
        printf("\033[1;32m[ Deltoid ]\033[0m stealth hook set. launch sober now.\n");
    } else {
        printf("\033[1;31m[ Error ]\033[0m failed to set flatpak overrides.\n");
    }

    return 0;
}
