/*  Moment Video Server - High performance media server
    Copyright (C) 2011 Dmitry Shatrov

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#ifndef __MOMENT__API__H__
#define __MOMENT__API__H__


// Stable C API for external modules.


#include <stdarg.h>


#ifdef __cplusplus
extern "C" {
#endif


// _________________________________ Messages __________________________________

typedef struct MomentMessage MomentMessage;

void moment_message_get_data (MomentMessage *message,
			      size_t         offset,
			      unsigned char *buf,
			      size_t         len);


// _______________________________ Stream events _______________________________

typedef struct MomentStreamHandler MomentStreamHandler;

typedef void (*MomentRtmpCommandMessageCallback) (MomentMessage *msg,
						  // TODO AMF decoder
						  void          *user_data);

typedef void (*MomentStreamClosedCallback) (void *user_data);

void moment_stream_handler_init (MomentStreamHandler *stream_handler);

void moment_stream_handler_set_command_message (MomentStreamHandler              *stream_handler,
						MomentRtmpCommandMessageCallback  cb,
						void                             *user_data);

void moment_stream_handler_set_closed (MomentStreamHandler        *stream_handler,
				       MomentStreamClosedCallback  cb,
				       void                       *user_data);


// ___________________________ Video stream control ____________________________

typedef void MomentStream;

typedef void *MomentStreamKey;

typedef void *MomentStreamHandlerKey;

MomentStream* moment_create_stream (void);

MomentStream* moment_get_stream (char const      *name_buf,
				 size_t           name_len,
				 MomentStreamKey *ret_stream_key,
				 unsigned         create);

void moment_stream_ref (MomentStream *stream);

void moment_stream_unref (MomentStream *stream);

void moment_remove_stream (MomentStreamKey stream_key);

MomentStreamHandlerKey moment_stream_add_handler (MomentStream        *stream,
						  MomentStreamHandler *stream_handler);

void moment_stream_remove_handler (MomentStream           *stream,
				   MomentStreamHandlerKey  stream_handler_key);


// _______________________________ Client events _______________________________

typedef struct MomentClientSession MomentClientSession;

void moment_client_session_ref (MomentClientSession *client_session);

void moment_client_session_unref (MomentClientSession *client_session);

void moment_client_session_disconnect (MomentClientSession *client_session);

typedef void (*MomentClientConnectedCallback) (MomentClientSession  *client_session,
					       char const           *app_name_buf,
					       size_t                app_name_len,
					       char const           *full_app_name_buf,
					       size_t                full_app_name_len,
					       void                **ret_client_user_data,
					       void                 *user_data);

typedef void (*MomentClientDisconnectedCallback) (void *client_user_data,
						  void *user_data);

typedef MomentStream* (*MomentStartWatchingCallback) (char const *stream_name_buf,
						      size_t      stream_name_len,
						      void       *client_user_data,
						      void       *user_data);

typedef MomentStream* (*MomentStartStreamingCallback) (char const *stream_name_buf,
						       size_t      stream_name_len,
						       void       *client_user_data,
						       void       *user_data);

typedef struct MomentClientHandler MomentClientHandler;

typedef void* MomentClientHandlerKey;

MomentClientHandler* moment_client_handler_new ();

void moment_client_handler_delete (MomentClientHandler *client_handler);

void moment_client_handler_set_connected (MomentClientHandler           *client_handler,
					  MomentClientConnectedCallback  cb,
					  void                          *user_data);

void moment_client_handler_set_disconnected (MomentClientHandler              *client_handler,
					     MomentClientDisconnectedCallback  cb,
					     void                             *user_data);

void moment_client_handler_set_start_watching (MomentClientHandler         *client_handler,
					       MomentStartWatchingCallback  cb,
					       void                        *user_data);

void moment_client_handler_set_start_streaming (MomentClientHandler          *client_handler,
						MomentStartStreamingCallback  cb,
						void                         *user_data);

MomentClientHandlerKey moment_add_client_handler (MomentClientHandler *client_handler,
						  char const          *prefix_bux,
						  size_t               prefix_len);

void moment_remove_client_handler (MomentClientHandlerKey client_handler_key);

void moment_client_send_command_message (MomentClientSession *client_session,
					 unsigned char const *msg_buf,
					 size_t               msg_len);


// _____________________________ Config file access ____________________________

typedef void MomentConfigSectionEntry;

typedef void MomentConfigSection;

typedef void MomentConfigOption;

typedef void *MomentConfigIterator;

// @section  Section to iterate through. Pass NULL to iterate through the root section.
MomentConfigIterator moment_config_section_iter_begin (MomentConfigSection *section);

MomentConfigSectionEntry* moment_config_section_iter_next (MomentConfigSection  *section,
							   MomentConfigIterator *iter);

MomentConfigSection* moment_config_section_entry_is_section (MomentConfigSectionEntry *section_entry);

MomentConfigOption* moment_config_section_entry_is_option (MomentConfigSectionEntry *section_entry);

size_t moment_config_option_get_value (MomentConfigOption *option,
				       char               *buf,
				       size_t              len);

size_t moment_config_get_option (char   *opt_path,
				 size_t  opt_path_len,
				 char   *buf,
				 size_t  len,
				 bool   *ret_is_set);


// __________________________________ Logging __________________________________

// The values are the same as for M::LogLevel.
typedef enum MomentLogLevel {
    MomentLogLevel_All      =  1000,
    MomentLogLevel_Debug    =  2000,
    MomentLogLevel_Info     =  3000,
    MomentLogLevel_Warning  =  4000,
    MomentLogLevel_Error    =  5000,
    MomentLogLevel_High     =  6000,
    MomentLogLevel_Failure  =  7000,
    MomentLogLevel_None     = 10000,
    MomentLogLevel_A        = MomentLogLevel_All,
    MomentLogLevel_D        = MomentLogLevel_Debug,
    MomentLogLevel_I        = MomentLogLevel_Info,
    MomentLogLevel_W        = MomentLogLevel_Warning,
    MomentLogLevel_E        = MomentLogLevel_Error,
    MomentLogLevel_H        = MomentLogLevel_High,
    MomentLogLevel_F        = MomentLogLevel_Failure,
    MomentLogLevel_N        = MomentLogLevel_None
} MomentLogLevel;

void moment_log  (MomentLogLevel log_level, char const *fmt, ...);
void moment_vlog (MomentLogLevel log_level, char const *fmt, va_list ap);

void moment_log_d (char const *fmt, ...);
void moment_log_i (char const *fmt, ...);
void moment_log_w (char const *fmt, ...);
void moment_log_e (char const *fmt, ...);
void moment_log_h (char const *fmt, ...);
void moment_log_f (char const *fmt, ...);


#ifdef __cplusplus
}
#endif


#include <moment/api_amf.h>


#endif /* __MOMENT__API__H__ */

