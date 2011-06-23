COMMON_CFLAGS =				\
	-D_POSIX_C_SOURCE=199309L	\
	-D_XOPEN_SOURCE=600		\
	-ggdb -pedantic			\
	-Wno-long-long -Wall -Wextra	\
	-DLIBMOMENT_PREFIX="\"$(libdir)\""	\
	$(THIS_CFLAGS)

if PLATFORM_WIN32
    COMMON_CFLAGS += -DPLATFORM_WIN32
else
    # For backtraces
    COMMON_CFLAGS += -rdynamic -pthread
# Moved to libMary.
#    COMMON_CFLAGS += -DLIBMARY_WRITEV
endif

AM_CFLAGS = -std=c99 $(COMMON_CFLAGS)
AM_CXXFLAGS = -std=c++0x $(COMMON_CFLAGS)

if PLATFORM_WIN32
else
# <fstream> doesn't work in strict ANSI mode with MinGW.
#AM_CXXFLAGS += -std=c++98 -ansi
endif

INCLUDES = -I$(top_srcdir)

moment_private_headers =

moment_target_headers =		\
	libmoment.h		\
				\
	amf_encoder.h		\
	amf_decoder.h		\
	rtmp_connection.h	\
	rtmp_server.h		\
	rtmp_video_service.h	\
	rtmp_service.h		\
	rtmpt_server.h		\
	rtmpt_service.h		\
				\
	video_stream.h		\
				\
	moment_server.h

libmomentincludedir = $(includedir)/moment-1.0/moment
libmomentinclude_HEADERS = $(moment_target_headers)

lib_LTLIBRARIES = libmoment-1.0.la
libmoment_1_0_la_SOURCES =	\
	libmoment.cpp		\
				\
	amf_encoder.cpp		\
	amf_decoder.cpp		\
	rtmp_connection.cpp	\
	rtmp_server.cpp		\
	rtmp_service.cpp	\
	rtmpt_server.cpp	\
	rtmpt_service.cpp	\
				\
	video_stream.cpp	\
				\
	moment_server.cpp

moment_extra_dist =

libmoment_1_0_la_LDFLAGS = -no-undefined -version-info "0:0:0"
libmoment_1_0_la_LIBADD = $(THIS_LIBS)
if PLATFORM_WIN32
libmoment_1_0_la_LIBADD += -lws2_32
endif

momentdir = $(libdir)/moment-1.0
moment_LTLIBRARIES = libmoment-rtmp-1.0.la
libmoment_rtmp_1_0_la_SOURCES =	\
	moment_rtmp.cpp

libmoment_rtmp_1_0_la_LDFLAGS = -no-undefined -version-info "0:0:0"
libmoment_rtmp_1_0_la_LIBADD = $(THIS_LIBS)
if PLATFORM_WIN32
libmoment_rtmp_1_0_la_LIBADD += -lws2_32
endif

bin_PROGRAMS = moment
moment_SOURCES =	\
	moment.cpp
moment_LDADD = $(top_builddir)/moment/libmoment-1.0.la $(THIS_LIBS)

EXTRA_DIST = $(moment_private_headers) $(moment_extra_dist)

