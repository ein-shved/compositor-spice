#include "monitor-renderer.h"

#include "pixman-renderer.h"
#include "compositor.h"

#include <wayland-server.h>

static struct monitor_renderer {
    struct weston_renderer renderer;
    struct weston_renderer *base;
} monitor_renderer;

int
__wrap_pixman_renderer_init(struct weston_compositor *ec);
int 
__real_pixman_renderer_init(struct weston_compositor *ec);

static void
monitor_renderer_flush_damage (struct weston_surface *surface)
{
    struct monitor_renderer *renderer = (struct monitor_renderer *)surface->compositor->renderer;
    pid_t pid;
    uid_t uid;

    if (surface->resource) {
        wl_client_get_credentials(wl_resource_get_client(surface->resource), &pid, &uid, NULL);

        weston_log("Flush damage on surface %X, client: (%u,%u)\n",
            wl_resource_get_id(surface->resource), uid, pid);
    }

    monitor_renderer.renderer.flush_damage(surface);
}

static void
monitor_renderer_attach(struct weston_surface *es, struct weston_buffer *buffer)
{
    struct monitor_renderer *renderer = (struct monitor_renderer *)es->compositor->renderer;
    pid_t pid;
    uid_t uid;

    if (es->resource) {
    
        wl_client_get_credentials(wl_resource_get_client(es->resource), &pid, &uid, NULL);

        weston_log("Attach buffer %X to surface %X, client: (%u,%u)\n", wl_resource_get_id(buffer->resource),
            wl_resource_get_id(es->resource), uid, pid);
    }

    monitor_renderer.renderer.attach(es, buffer);   
}
static int
monitor_renderer_create_surface(struct weston_surface *surface)
{
    struct monitor_renderer *renderer = (struct monitor_renderer *)surface->compositor->renderer;
    pid_t pid = 0;
    uid_t uid = 0;
    int ret;
    
    if (surface->resource) {
        wl_client_get_credentials(wl_resource_get_client(surface->resource), &pid, &uid, NULL);
    
        weston_log("Create surface %X, client: (%u,%u)\n",
            wl_resource_get_id(surface->resource), uid, pid);
    } else {
        weston_log("Create surface.\n");
    }

    ret = monitor_renderer.renderer.create_surface(surface);
    return ret;
}

static void
monitor_renderer_surface_set_color(struct weston_surface *es,
		 float red, float green, float blue, float alpha)
{
    struct monitor_renderer *renderer = (struct monitor_renderer *)es->compositor->renderer;
    pid_t pid;
    uid_t uid;

    if (es->resource) {
        wl_client_get_credentials(wl_resource_get_client(es->resource), &pid, &uid, NULL);
    
        weston_log("Set color of surface %X, client: (%u,%u)\n",
            wl_resource_get_id(es->resource), uid, pid);
    }

    monitor_renderer.renderer.surface_set_color(es, red, green, blue, alpha);
}
static void
monitor_renderer_destroy_surface(struct weston_surface *es)
{
    struct monitor_renderer *renderer = (struct monitor_renderer *)es->compositor->renderer;
    pid_t pid;
    uid_t uid;

    if (es->resource) {
        wl_client_get_credentials(wl_resource_get_client(es->resource), &pid, &uid, NULL);
    
        weston_log("Destroy surface %X, client: (%u,%u)\n",
            wl_resource_get_id(es->resource), uid, pid);
    }

    monitor_renderer.renderer.destroy_surface(es);
}
static void
monitor_renderer_destroy(struct weston_compositor *ec)
{
    struct monitor_renderer *renderer = (struct monitor_renderer *)ec->renderer;

    weston_log("Destroy renderer\n");

    monitor_renderer.renderer.destroy(ec);
}
static int
monitor_renderer_postinit (struct weston_compositor *ec)
{
    monitor_renderer.renderer = *(ec->renderer);
    monitor_renderer.base = ec->renderer;
    ec->renderer->read_pixels = ec->renderer->read_pixels;// monitor_renderer_read_pixels;
	ec->renderer->repaint_output = ec->renderer->repaint_output;// monitor_renderer_repaint_output;
	ec->renderer->flush_damage = monitor_renderer_flush_damage;
	ec->renderer->attach = monitor_renderer_attach;
	ec->renderer->create_surface = monitor_renderer_create_surface;
	ec->renderer->surface_set_color = monitor_renderer_surface_set_color;
	ec->renderer->destroy_surface = monitor_renderer_destroy_surface;
	ec->renderer->destroy = monitor_renderer_destroy;

    return 0;
}

WL_EXPORT int 
monitor_renderer_init(struct weston_compositor *ec)
{
    int ret = 0;
    if ( (ret = __real_pixman_renderer_init(ec)) <0) {
        return ret;
    }
    
    return monitor_renderer_postinit(ec);
}

WL_EXPORT int
__wrap_pixman_renderer_init(struct weston_compositor *ec)
{
    int ret = 0;

    weston_log("Monitor renderer in front of pixman renderer."
            " Formant of client output is: client: (uid,pid).\n");

    if ( (ret = __real_pixman_renderer_init(ec)) <0) {
        return ret;
    }
    
    return monitor_renderer_postinit(ec);
}

