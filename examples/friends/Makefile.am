noinst_PROGRAMS := friends

friends_SOURCES = main.c
friends_CFLAGS = $(LIBGFBGRAPH_CFLAGS) $(GOA_CFLAGS) $(GDK_PIXBUF_CFLAGS)
friends_LDADD = $(top_builddir)/gfbgraph/libgfbgraph-@API_VERSION@.la $(LIBGFBGRAPH_LIBS) $(GOA_LIBS) $(GDK_PIXBUF_LIBS)

exampledir = $(pkgdatadir)/examples/friends
example_DATA = main.c
