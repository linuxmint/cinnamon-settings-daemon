NULL =

noinst_LTLIBRARIES = libgvc.la

INCLUDES =					\
	$(WARN_CFLAGS)				\
	$(MEDIA_KEYS_CFLAGS)			\
	$(NULL)

libgvc_la_LIBADD =		\
	$(MEDIA_KEYS_LIBS)	\
	$(NULL)

libgvc_la_SOURCES =			\
	gvc-mixer-stream.h		\
	gvc-mixer-stream.c		\
	gvc-mixer-stream-private.h	\
	gvc-channel-map.h		\
	gvc-channel-map.c		\
	gvc-channel-map-private.h	\
	gvc-mixer-card.c		\
	gvc-mixer-card.h		\
	gvc-mixer-card-private.h	\
	gvc-mixer-sink.h		\
	gvc-mixer-sink.c		\
	gvc-mixer-source.h		\
	gvc-mixer-source.c		\
	gvc-mixer-sink-input.h		\
	gvc-mixer-sink-input.c		\
	gvc-mixer-source-output.h	\
	gvc-mixer-source-output.c	\
	gvc-mixer-event-role.h		\
	gvc-mixer-event-role.c		\
	gvc-mixer-control.h		\
	gvc-mixer-control.c		\
	gvc-mixer-control-private.h	\
	gvc-pulseaudio-fake.h		\
	$(NULL)

MAINTAINERCLEANFILES =                  \
        *~                              \
        Makefile.in
