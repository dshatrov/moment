COMMON_CFLAGS =					\
	-ggdb					\
	-Wno-long-long -Wall -Wextra		\
	-DLIBMOMENT_PREFIX="\"$(libdir)\""	\
	$(THIS_CFLAGS)

if PLATFORM_WIN32
    COMMON_CFLAGS += -DPLATFORM_WIN32
endif
if PLATFORM_CYGWIN
    COMMON_CFLAGS += -DPLATFORM_CYGWIN -pthread
endif

if PLATFORM_DEFAULT
    COMMON_CFLAGS += -DPLATFORM_DEFAULT -pthread
    # For backtraces
    COMMON_CFLAGS += -rdynamic
endif

COMMON_LDFLAGS =

AM_CFLAGS = -std=gnu99 $(COMMON_CFLAGS)
AM_CXXFLAGS = -std=gnu++0x $(COMMON_CFLAGS)

INCLUDES = -I$(top_srcdir)

moment_private_headers =

moment_target_headers =		\
	libmoment.h		\
				\
	moment_types.h		\
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
	av_recorder.h		\
	av_muxer.h		\
	flv_muxer.h		\
				\
	storage.h		\
	local_storage.h		\
				\
        push_protocol.h         \
        rtmp_push_protocol.h    \
        push_agent.h            \
                                \
	moment_server.h		\
				\
	api.h			\
				\
	module_init.h

#	flv_file_source.h

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
	av_recorder.cpp		\
	flv_muxer.cpp		\
				\
	local_storage.cpp	\
				\
        rtmp_push_protocol.cpp  \
        push_agent.cpp          \
                                \
	moment_server.cpp	\
				\
	api.cpp			\
	api_amf.cpp		\
				\
	hmac/sha2.c		\
	hmac/hmac_sha2.c        \
                                \
        tsmux/tsmux.c           \
        tsmux/tsmuxstream.c

#	flv_file_source.cpp
 
moment_extra_dist =

libmoment_1_0_la_LDFLAGS = -no-undefined -version-info "0:0:0" $(COMMON_LDFLAGS)
libmoment_1_0_la_LIBADD = $(THIS_LIBS)
if PLATFORM_WIN32
libmoment_1_0_la_LIBADD += -lws2_32
endif

momentdir = $(libdir)/moment-1.0
moment_LTLIBRARIES = libmoment-file-1.0.la libmoment-rtmp-1.0.la libmoment-test-1.0.la # libmoment-hls-1.0.la # libmoment-archive-1.0.la

libmoment_file_1_0_la_DEPENDENCIES = libmoment-1.0.la
libmoment_file_1_0_la_SOURCES = \
	mod_file.cpp
libmoment_file_1_0_la_LDFLAGS = -no-undefined -version-info "0:0:0" $(COMMON_LDFLAGS)
libmoment_file_1_0_la_LIBADD = $(top_builddir)/moment/libmoment-1.0.la $(THIS_LIBS)

libmoment_rtmp_1_0_la_DEPENDENCIES = libmoment-1.0.la
libmoment_rtmp_1_0_la_SOURCES =	\
	moment_rtmp.cpp
libmoment_rtmp_1_0_la_LDFLAGS = -no-undefined -version-info "0:0:0" $(COMMON_LDFLAGS)
libmoment_rtmp_1_0_la_LIBADD = $(top_builddir)/moment/libmoment-1.0.la $(THIS_LIBS)
if PLATFORM_WIN32
libmoment_rtmp_1_0_la_LIBADD += -lws2_32
endif

#libmoment_archive_1_0_la_DEPENDENCIES = libmoment-1.0.la
#libmoment_archive_1_0_la_SOURCES = \
#	mod_archive.cpp
#libmoment_archive_1_0_la_LDFLAGS = -no-undefined -version-info "0:0:0" $(COMMON_LDFLAGS)
#libmoment_archive_1_0_la_LIBADD = $(top_builddir)/moment/libmoment-1.0.la $(THIS_LIBS)

#libmoment_hls_1_0_la_DEPENDENCIES = libmoment-1.0.la
#libmoment_hls_1_0_la_SOURCES = \
#	mod_hls.cpp
#libmoment_hls_1_0_la_LDFLAGS = -no-undefined -version-info "0:0:0" $(COMMON_LDFLAGS)
#libmoment_hls_1_0_la_LIBADD = $(top_builddir)/moment/libmoment-1.0.la $(THIS_LIBS)

libmoment_test_1_0_la_DEPENDENCIES = libmoment-1.0.la
libmoment_test_1_0_la_SOURCES =	\
	mod_test.cpp
libmoment_test_1_0_la_LDFLAGS = -no-undefined -version-info "0:0:0" $(COMMON_LDFLAGS)
libmoment_test_1_0_la_LIBADD = $(top_builddir)/moment/libmoment-1.0.la $(THIS_LIBS)

bin_PROGRAMS = moment rtmptool

moment_DEPENDENCIES = libmoment-1.0.la
moment_SOURCES =	\
	moment.cpp
moment_LDADD = $(top_builddir)/moment/libmoment-1.0.la $(THIS_LIBS)
moment_LDFLAGS = $(COMMON_LDFLAGS)

rtmptool_DEPENDENCIES = libmoment-1.0.la
rtmptool_SOURCES =	\
	rtmptool.cpp
rtmptool_LDADD = $(top_builddir)/moment/libmoment-1.0.la $(THIS_LIBS)
rtmptool_LDFLAGS = $(COMMON_LDFLAGS)

EXTRA_DIST = $(moment_private_headers) $(moment_extra_dist)

myplayerdir = $(datadir)/moment
myplayer_DATA = moment.example.conf

