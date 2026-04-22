#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <string.h>

int main() {
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;
    char cmd[1024];

    printf("\033[1;36m[Deltoid]\033[0m init [ Deltoid ]..\n");

    // grant Sober access to your home directory for the .so and scripts
    snprintf(cmd, sizeof(cmd), "flatpak override --user --filesystem=%s org.vinegarhq.Sober", homedir);
    system(cmd);

    // set the LD_PRELOAD to point to your compiled library
    snprintf(cmd, sizeof(cmd), "flatpak override --user --env=LD_PRELOAD=%s/libdeltoid.so org.vinegarhq.Sober", homedir);
    
    if (system(cmd) == 0) {
        printf("\033[1;32m[Deltoid]\033[0m hooked, open sober to start.\n");
    } else {
        printf("\033[1;31m[Error]\033[0m could not set flatpak overrides.\n");
    }

    return 0;
}
