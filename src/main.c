#include "app.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    App app;

    if (!app_init(&app)) {
        fprintf(stderr, "Failed to initialize application\n");
        app_destroy(&app);
        return 1;
    }

    app_run(&app);
    app_destroy(&app);

    return 0;
}