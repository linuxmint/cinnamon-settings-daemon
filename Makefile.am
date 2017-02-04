NULL =

SUBDIRS = 			\
	cinnamon-settings-daemon	\
	plugins			\
	data			\
	po 			\
	$(NULL)

if ENABLE_MAN
SUBDIRS += man
endif

# Honor aclocal flags
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4

EXTRA_DIST = \
	MAINTAINERS		\
	ChangeLog		\
	README			\
	autogen.sh \
	$(NULL)

DISTCLEANFILES = \
	$(NULL)
