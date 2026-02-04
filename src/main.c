#include "registry.h"
#include "cleanup.h"
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <poll.h>
#include <wayland-client-core.h>
#include <wayland-util.h>

static const char usage[] =
	"Usage: " PROJECT_NAME " [options...] [output-file]\n"
	"  -h, --help            Show help message and quit\n"
	"  -o, --output          Set the output name to capture\n";

static bool read_args(struct app_state *app, int argc, char *argv[]) {
    int c;
    int option_index = 0;

    static struct option long_options[] = {
        {"help",   no_argument,       nullptr, 'h'},
        {"output", required_argument, nullptr, 'o'},
        {nullptr,  0,                 nullptr,  0 }
    };

    while ((c = getopt_long(argc, argv, "ho:", long_options, &option_index)) != -1) {
        if (c == 'o') {
            app->f_output = optarg;
        } else {
            printf(usage);
            return false;
        }
    }

	if (optind < argc - 1) {
	    printf(usage);
        return false;
	} else if (optind == argc - 1) {
		app->f_path = argv[optind];
	}

    return true;
}

static bool setup_main(struct app_state *app) {
    // Инициализация состояния приложения
    wl_list_init(&app->outputs);
    app->running = true;

    // Инициализация XKB контекста для клавиатуры
    app->xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!app->xkb_ctx) {
        fprintf(stderr, "Failed to create XKB context\n");
        return false;
    }

    // Подключение к Wayland дисплею
    app->display = wl_display_connect(nullptr);
    if (!app->display) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        return false;
    }

    // Получение registry и регистрация listener'а
    app->registry = wl_display_get_registry(app->display);
    wl_registry_add_listener(app->registry, &registry_listener, app);

    // Первый roundtrip для получения глобалов и мониторов
    wl_display_roundtrip(app->display);

    // Проверка наличия необходимых протоколов
    if (!app->compositor) {
        fprintf(stderr, "Compositor doesn't support wl_compositor\n");
        return false;
    }
    if (!app->layer_shell) {
        fprintf(stderr, "Compositor doesn't support zwlr_layer_shell_v1\n");
        return false;
    }
    if (!app->shm) {
        fprintf(stderr, "Compositor doesn't support wl_shm\n");
        return false;
    }
    if (!app->screencopy_manager) {
        fprintf(stderr, "Compositor doesn't support zwlr_screencopy_manager_v1\n");
        return false;
    }

    // Второй roundtrip для вызова слушателей
    wl_display_roundtrip(app->display);

    if (app->f_output && !app->screencopy_called) {
        fprintf(stderr, "Display %s not available\n", app->f_output);
        return false;
    }

    return true;
}

static int loop(struct app_state *app) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &mask, nullptr);

    int sfd = signalfd(-1, &mask, SFD_NONBLOCK);

    struct pollfd fds[2] = {
        { wl_display_get_fd(app->display), POLLIN, 0 },
        { sfd, POLLIN, 0 }
    };

    while (app->running) {
        while (wl_display_prepare_read(app->display) != 0) {
            wl_display_dispatch_pending(app->display);
        }
        wl_display_flush(app->display);

        if (poll(fds, 2, -1) == -1 || fds[1].revents & POLLIN) {
            wl_display_cancel_read(app->display);
            close(sfd);
            return false;
        }

        if (fds[0].revents & POLLIN) {
            wl_display_read_events(app->display);
        } else {
            wl_display_cancel_read(app->display);
        }

        wl_display_dispatch_pending(app->display);
    }

    close(sfd);
    return true;
}

int main(int argc, char *argv[]) {
    struct app_state app;
    memset(&app, 0, sizeof(app));
    int ret = 1;

    if (!read_args(&app, argc, argv)) goto cleanup;
    if (!setup_main(&app)) goto cleanup;
    if (!loop(&app)) goto cleanup;

    ret = 0;
cleanup:
    cleanup_app(&app);
    return ret;
}
