AM_CPPFLAGS =					\
	-I$(top_srcdir)/shared			\
	-I$(top_builddir)/src			\
	-DDATADIR='"$(datadir)"'		\
	-DMODULEDIR='"$(moduledir)"'		\
    -DIN_WESTON                     \
	-DLIBEXECDIR='"$(libexecdir)"'		\
	-DSPICE_PATH='"@SPICE_PATH@"'

moduledir = @libdir@/weston
module_LTLIBRARIES = spice-backend.la

spice_backend = spice-backend.la
spice_backend_la_LDFLAGS = -module -avoid-version
spice_backend_la_LIBADD = $(COMPOSITOR_LIBS) $(SPICE_LIBS)  \
    $(top_srcdir)/shared/libshared.la

spice_backend_la_CFLAGS =       \
	$(COMPOSITOR_CFLAGS)		\
	$(PIXMAN_CFLAGS)			\
	$(SPICE_CFLAGS)	            \
	$(GCC_CFLAGS)
spice_backend_la_SOURCES =      \
    compositor-spice.c          \
    compositor-spice.h          \
    compositor-spice-conf.h     \
    weston_basic_event_loop.c   \
    weston_basic_event_loop.h   \
    weston_spice_interfaces.h   \
    weston_input_interfaces.c   \
    weston_qxl_interface.c      \
    weston_qxl_commands.h       \
    weston_qxl_commands.c
