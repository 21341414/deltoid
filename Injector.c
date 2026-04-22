#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <string.h>

int main() {
    // linux username
    struct passwd *pw = getpwuid(getuid());
    const char *user = pw->pw_name;

    printf("\033[1;34m[Deltoid]\033[0m starting environment for: %s\n", user);

    // wipe old flatpak overrides to prevent "environment pollution" ( dtc risk )
    system("flatpak override --user --reset org.vinegarhq.Sober > /dev/null 2>&1");

    // set sandbox perms
    // allows Sober to read the .so from your linux home folder
    char fs_cmd[256];
    sprintf(fs_cmd, "flatpak override --user --filesystem=/home/%s org.vinegarhq.Sober", user);
    system(fs_cmd);

    // injec via LD_PRELOAD
    char env_cmd[512];
    sprintf(env_cmd, "flatpak override --user --env=LD_PRELOAD=/home/%s/libdeltoid.so org.vinegarhq.Sober", user);
    
    if (system(env_cmd) == 0) {
        printf("\033[1;32m[Deltoid] SUCCESS:\033[0m hook injected.\n");
        printf("\033[1;33m[Action Required]:\033[0m open sober now. Deltoid will auto run.\n");
    } else {
        printf("\033[1;31m[Error]:\033[0m failed to set flatpak overrides, is sober installed?\n");
    }

    return 0;
}
