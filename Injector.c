#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>

int main() {
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;

    printf("[ Deltoid ] KX Launcher \n");

    // grant sober permission to see the .so in your home folder
    char fs_cmd[256];
    snprintf(fs_cmd, sizeof(fs_cmd), "flatpak override --user --filesystem=%s org.vinegarhq.Sober", homedir);
    system(fs_cmd);

    // set the LD_PRELOAD env variable
    char env_cmd[512];
    snprintf(env_cmd, sizeof(env_cmd), "flatpak override --user --env=LD_PRELOAD=%s/libdeltoid.so org.vinegarhq.Sober", homedir);
    
    if (system(env_cmd) == 0) {
        printf("[+] hook successful.\n");
        printf("[!] launch sober now to execute.\n");
    } else {
        printf("[-] failed to set env variable.\n");
    }

    return 0;
}
