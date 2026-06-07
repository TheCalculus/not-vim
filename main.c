#include "events.h"
#include "nvapi.h"
#include <assert.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define CVECTOR_LOGARITHMIC_GROWTH

#include "arena.h"
#include "buffer.h"
#include "editor.h"
#include "error.h"
#include "window.h"
#include <tui.h>

static int nv_open_file_in_window(struct nv_editor* editor, const char* filename);
static void nv_fatal_signal(int sig, siginfo_t* info, void* ucontext);
static void nv_editor_cleanup(struct nv_editor* editor);
static void nv_cleanup();
static void nv_setup_signal_handlers();
static void nv_must_be_no_errors(const char* message);
static int nvrpc_load(struct nv_api* nv_api);
static int nvlua_load(struct nv_api* nv_api);
static int get_exe_path(char* buf, size_t bufsiz);

static int nv_open_file_in_window(struct nv_editor* editor, const char* filename)
{
    nv_log("opening argument filepath %s\n", filename);

    struct nv_view* view = nv_view_init(filename);
    if (!view) {
        return NV_ERR_MEM;
    }

    struct nv_window_node* window = nv_window_node_init(NV_WM_VIEW);
    if (!window) {
        nv_free_view(view);
        return NV_ERR_MEM;
    }

    window->leaf.view = view;
    nv_window_set_focus(nv_window_node_push_child(editor->focus, window));

    return editor->status;
}

static void nv_editor_cleanup(struct nv_editor* editor)
{
    nv_tui_free();
    nv_free_views();
    nv_free_windows();
    editor->logger = NULL;
    nv_event_free();
    editor->focus = NULL;
    editor->window = NULL;

    for (int i = 0; i < NV_POLLER_COUNT; i++) {
        if (!editor->pollers[i]) {
            continue;
        }
        free(editor->pollers[i]);
    }

    if (editor->nvlua) {
        editor->nvlua->nvlua_free();
        (void)dlclose(editor->nvlua->dl_handle);
    }

    if (editor->nvrpc) {
        editor->nvrpc->nvrpc_free();
        (void)dlclose(editor->nvrpc->dl_handle);
    }
}

static void nv_cleanup()
{
    nv_tui_clear();
    int rv = nv_editor->status;
    if (rv != NV_OK) {
        fprintf(stderr, "%s (%d) issued by %s@%s\n", nv_strerror(rv), rv, nv_editor->setter_func, nv_editor->setter_line);
    }
    if (nv_editor) {
        nv_editor_cleanup(nv_editor);
    }
    nv_arena_free_all();
}

static void nv_fatal_signal(int sig, siginfo_t* info, void* ucontext)
{
    (void)ucontext;
    nv_tui_free();

    const char* sig_name;

    switch(sig) {
        case SIGSEGV: sig_name = "SIGSEGV"; break;
        case SIGBUS:  sig_name = "SIGBUS";  break;
        case SIGFPE:  sig_name = "SIGFPE";  break;
        case SIGABRT: sig_name = "SIGABRT"; break;
        case SIGILL:  sig_name = "SIGILL"; break;
        case SIGTRAP: sig_name = "SIGTRAP"; break;
        default:      sig_name = "UNKNOWN"; break;
    }

    fprintf(stderr, "%s (%d) at address %p\n", sig_name, sig, info->si_addr);

    void* bt[32];
    int bt_size = backtrace(bt, 32);
    backtrace_symbols_fd(bt, bt_size, STDERR_FILENO);

    exit(128 + sig);
}

static void nv_setup_signal_handlers()
{
    atexit(nv_cleanup);

    signal(SIGTTOU, SIG_IGN);

    struct sigaction sa;
    sa.sa_sigaction = nv_fatal_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND;

    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS,  &sa, NULL);
    sigaction(SIGFPE,  &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
}

static void nv_must_be_no_errors(const char* message)
{
    if (nv_editor->status != NV_OK) {
        nv_fatal(message);
        exit(nv_editor->status);
    }
}

static void nv_init_base_window()
{
    nv_editor->window = nv_window_node_init(NV_WM_SPLIT);
    nv_must_be_no_errors("failed to create editor base window\n");
    assert(nv_editor->window->kind == NV_WM_SPLIT);
    nv_editor->window->split.kind = NV_SPLIT_HORIZONTAL;
    nv_editor->window->split.ratio = 0.98;
}

static void nv_init_logger_window()
{
    nv_editor->logger = nv_window_node_init(NV_WM_VIEW);
    nv_must_be_no_errors("failed to create log window\n");
    nv_editor->logger->leaf.view = nv_view_init(NULL);
    nv_must_be_no_errors("failed to create log buffer\n");

    struct nv_context logger_ctx = nv_get_context(nv_editor->logger);
    logger_ctx.buffer->type = NV_BUFF_TYPE_LOG;
    logger_ctx.buffer->format = NV_FILE_FORMAT_PLAINTEXT;
    logger_ctx.view->visible = false;
}

static void nv_init_primary_window()
{
    assert(nv_editor->window->kind == NV_WM_SPLIT);
    struct nv_window_node* primary = nv_window_node_init(NV_WM_VIEW);
    nv_must_be_no_errors("failed to create primary window\n");
    nv_editor->window->split.left = primary;
}

static void nv_init_status_window()
{
    assert(nv_editor->window->kind == NV_WM_SPLIT);
    struct nv_window_node* status = nv_window_node_init(NV_WM_VIEW);
    nv_must_be_no_errors("failed to create status window\n");
    status->leaf.view = nv_view_init(NULL);
    nv_must_be_no_errors("failed to create status view\n");
    status->leaf.view->allow_split = false;
    status->leaf.view->gutter_width_cols = 0;
    nv_editor->window->split.right = status;
    nv_editor->statline = status;
}

static void nv_open_input_files(int argc, char** argv)
{
    for (int i = 1; i < argc; i++) {
        if (nv_open_file_in_window(nv_editor, (const char*)argv[i]) != NV_OK) {
            nv_fatal("failed to open a file\n");
            exit(nv_editor->status);
        }
    }
}

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

// returns path of folder which contains nv
static int get_exe_path(char* buf, size_t bufsiz) {
#if defined(__APPLE__)
    uint32_t size = (uint32_t)bufsiz;
    char exe_path[PATH_MAX];

    if (_NSGetExecutablePath(exe_path, &size) != 0) {
        return -1;
    }

    char resolved[PATH_MAX];
    if (realpath(exe_path, resolved) == NULL) {
        return -1;
    }

    char* last_slash = strrchr(resolved, '/');
    if (!last_slash) {
        return -1;
    }

    size_t dirlen = (size_t)(last_slash - resolved);
    if (dirlen == 0) {
        if (bufsiz < 2) {
            return -1;
        }
        strcpy(buf, "/");
        return 1;
    }

    if (dirlen >= bufsiz) {
        return -1;
    }

    memcpy(buf, resolved, dirlen);
    buf[dirlen] = '\0';
    return (int)dirlen;
#elif defined(__linux__)
    ssize_t len = readlink("/proc/self/exe", buf, bufsiz - 1);
    if (len <= 0 || (size_t)len >= bufsiz) {
        return -1;
    }

    buf[len] = '\0';

    char* last_slash = strrchr(buf, '/');
    if (!last_slash) {
        return -1;
    }

    size_t dirlen = (size_t)(last_slash - buf);
    if (dirlen == 0) {
        if (bufsiz < 2) {
            return -1;
        }
        strcpy(buf, "/");
        return 1;
    }

    buf[dirlen] = '\0';
    return (int)dirlen;
#else
#warning "UNSUPPORTED"
    (void)buf; (void)bufsiz;
    return -1;
#endif
}

static nv_plugin_init_t nv_call_plugin_init(void* handle, const char* symbol)
{
    nv_plugin_init_t plugin_init = (nv_plugin_init_t)dlsym(handle, symbol);

    if (!plugin_init) {
        nv_log("failed to find %s\n", symbol);
        dlclose(handle);
        return NULL;
    }

    return plugin_init;
}

static int nvrpc_load(struct nv_api* nv_api)
{
    char buf[128];
    int bufsiz = sizeof(buf) / sizeof(char);
    size_t nread = get_exe_path(buf, bufsiz);

    if (nread < 0) {
        return NV_ERR;
    }

    const char libnvrpc_path[] =
        "/"
        #ifdef __linux__
            "libnvrpc.so"
        #elif defined(__APPLE__)
            "libnvrpc.dylib"
        #endif
    ;

    if (snprintf(buf + nread, bufsiz - nread, "%s", libnvrpc_path) >= bufsiz - nread) {
        return NV_ERR;
    }
    void* nvrpc_handle = dlopen(buf, RTLD_NOW);

    if (!nvrpc_handle) {
        nv_log("nvrpc not found at %s\n", buf);
        nv_log("nvrpc is required for full nvlua functionality\n");
        return NV_ERR;
    }

    nv_log("nvrpc found and loaded\n");

    nv_plugin_init_t plug_init;
    if (!(plug_init = nv_call_plugin_init(nvrpc_handle, "nvrpc_plugin_init"))) {
        nv_log("nvrpc plugin_init not found\n");
        dlclose(nvrpc_handle);
        return NV_ERR;
    }

    struct nvrpc_api* nvrpc = (struct nvrpc_api*)plug_init(0, nv_api);

    if (!nvrpc) {
        nv_log("nvrpc plugin_init failed\n");
        dlclose(nvrpc_handle);
        return NV_ERR;
    }

    nvrpc->dl_handle = nvrpc_handle;
    nv_editor->nvrpc = nvrpc;
    (void)nv_editor->nvrpc->nvrpc_main();

    return NV_OK;
}

static int nvlua_load(struct nv_api* nv_api)
{
    if (!nv_editor->nvrpc) {
        // don't bother loading lua if rpc isn't running
        nv_log("nvlua requires nvrpc for lua functionality, aborting\n");
        return NV_ERR;
    }

    char buf[128];
    int bufsiz = sizeof(buf) / sizeof(char);
    size_t nread = get_exe_path(buf, bufsiz);

    if (nread < 0) {
        return NV_ERR;
    }

    const char libnvlua_path[] =
        "/"
        #ifdef __linux__
            "libnvlua.so"
        #elif defined(__APPLE__)
            "libnvlua.dylib"
        #endif
    ;

    if (snprintf(buf + nread, bufsiz - nread, "%s", libnvlua_path) >= bufsiz - nread) {
        return NV_ERR;
    }
    void* nvlua_handle = dlopen(buf, RTLD_NOW);

    if (!nvlua_handle) {
        nv_log("nvlua not found at %s\n", buf);
        return NV_ERR;
    }

    nv_log("nvlua found and loaded\n");

    nv_plugin_init_t plug_init;
    if (!(plug_init = nv_call_plugin_init(nvlua_handle, "nvlua_plugin_init"))) {
        nv_log("nvlua plugin_init not found\n");
        dlclose(nvlua_handle);
        return NV_ERR;
    }

    struct nvlua_api* nvlua = (struct nvlua_api*)plug_init(0, nv_api);

    if (!nvlua) {
        nv_log("nvlua plugin_init failed\n");
        dlclose(nvlua_handle);
        return NV_ERR;
    }

    nvlua->dl_handle = nvlua_handle;
    nv_editor->nvlua = nvlua;
    (void)nv_editor->nvlua->nvlua_main();

    return NV_OK;
}

static struct nv_conf* nv_get_config()
{
    return &nv_editor->config;
}

int main(int argc, char** argv)
{
    nv_setup_signal_handlers();
    nv_arena_init();

    struct nv_editor editor = { 0 };
    if (nv_editor_init(&editor) != NV_OK) {
        nv_editor_cleanup(&editor);
        return editor.status;
    }

    assert(nv_editor);

    if (!editor.config.show_headless && (editor.status = nv_tui_init()) != NV_OK) {
        fprintf(stderr, "%s\n", nv_strerror(editor.status));
        nv_editor_cleanup(&editor);
        return editor.status;
    }

    nv_resize_for_layout(nv_tui_width(), nv_tui_height());
    nv_init_base_window();
    nv_init_logger_window();
#ifdef NV_DEBUG_OPEN_LOG
    nv_editor->logger->leaf.view->visible = true;
#endif
    nv_init_primary_window();
    nv_init_status_window();
    nv_window_set_focus(nv_editor->window->split.left); // set primary window as focus

    struct nv_api nv_api = {
        .nv_log = nv_log,
        .nv_event_register_sub = nv_event_register_sub,
        .nv_event_str = nv_event_str,
        .nv_str_event = nv_str_event,
        .nv_get_config = nv_get_config,
    };

    if (nvrpc_load(&nv_api) != NV_OK) {
        nv_log("nvrpc failed to load\n");
    }
    
    if (nvlua_load(&nv_api) != NV_OK) {
        nv_log("nvlua failed to load\n");
    }

    nv_log("notvim initialised\n");

    if (argc > 1) {
        nv_open_input_files(argc, argv);
    }

    nv_main();
    exit(editor.status);
    return editor.status;
}
