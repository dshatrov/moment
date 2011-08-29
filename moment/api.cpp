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


#include <libmary/types.h>
#include <cstdio>

#include <libmary/libmary.h>
#include <moment/libmoment.h>
#include <mconfig/mconfig.h>

#include <moment/api.h>


// This file deals with data structures with overlapping meaning from three
// different domains. A naming convention should be applied to help identifying
// which symbol refers to which domain. The domains:
//
//   1. C API - "External" domain.
//   2. Moment's internal C++ interfaces - "Internal" domain.
//   3. Helper types and data of this API binding - "API" domain.
//
// Naming convention:
//
//   * All internal API types have "Api_" prefix in their names.
//   * Variables of API types have "api_" prefix.
//   * Variables of external types have "ext_" prefix.
//   * Variables of internal types have "int_" prefix.


using namespace M;
using namespace Moment;


#define MOMENT_API_INITIALIZED_MAGIC 0x1234


extern "C" {


// _________________________________ Messages __________________________________

struct MomentMessage {
    PagePool::PageListArray *pl_array;
};

#if 0
void moment_array_get (MomentArray   *array,
		       size_t         offset,
		       unsigned char *buf,
		       size_t         len)
{
}
#endif

#if 0
// Unnecessary
void moment_array_set (MomentArray         *array,
		       size_t               offset,
		       unsigned char const *buf,
		       size_t               len)
{
}
#endif

//MomentArray* moment_message_get_array (MomentMessage *message);

void moment_message_get_data (MomentMessage * const message,
			      size_t          const offset,
			      unsigned char * const buf,
			      size_t          const len)
{
    message->pl_array->get (offset, Memory (buf, len));
}


// _______________________________ Stream events _______________________________

struct MomentStreamHandler {
    unsigned initialized;

    MomentRtmpCommandMessageCallback command_cb;
    void *command_cb_data;

    MomentStreamClosedCallback closed_cb;
    void *closed_cb_data;
};

void moment_stream_handler_init (MomentStreamHandler * const stream_handler)
{
    stream_handler->command_cb = NULL;
    stream_handler->command_cb_data = NULL;

    stream_handler->closed_cb = NULL;
    stream_handler->closed_cb_data = NULL;

    stream_handler->initialized = MOMENT_API_INITIALIZED_MAGIC;
}

void moment_stream_handler_set_rtmp_command_message (MomentStreamHandler              * const stream_handler,
						     MomentRtmpCommandMessageCallback   const cb,
						     void                             * const user_data)
{
    stream_handler->command_cb = cb;
    stream_handler->command_cb_data = user_data;
}

void moment_stream_handler_set_closed (MomentStreamHandler * const stream_handler,
				       MomentStreamClosedCallback   const cb,
				       void * const user_data)
{
    stream_handler->closed_cb = cb;
    stream_handler->closed_cb_data = user_data;
}


// ___________________________ Video stream control ____________________________

MomentStream* moment_create_stream (void)
{
    Ref<VideoStream> video_stream = grab (new VideoStream);
    MomentStream * const stream = static_cast <MomentStream*> (video_stream);
    video_stream.setNoUnref ((VideoStream*) NULL);
    return stream;
}

MomentStream* moment_get_stream (char const      * const name_buf,
				 size_t            const name_len,
				 MomentStreamKey * const ret_stream_key,
				 unsigned          const create)
{
    MomentServer * const moment = MomentServer::getInstance ();

    MomentServer::VideoStreamKey video_stream_key;
    // TODO ret_video_stream_key in getVideoStream()?
    // ...not very nice.
    Ref<VideoStream> video_stream = moment->getVideoStream (ConstMemory (name_buf, name_len));
    if (!video_stream) {
	// FIXME There's a logical race condition here. It'd be better to get away without one.
	if (!create)
	    return NULL;

	video_stream = grab (new VideoStream);
	video_stream_key = moment->addVideoStream (video_stream, ConstMemory (name_buf, name_len));
    }

    if (ret_stream_key)
	*ret_stream_key = video_stream_key.getAsVoidPtr();

    MomentStream * const stream = static_cast <MomentStream*> (video_stream);
    video_stream.setNoUnref ((VideoStream*) NULL);
    return stream;
}

void moment_stream_ref (MomentStream * const stream)
{
    static_cast <VideoStream*> (stream)->ref ();
}

void moment_stream_unref (MomentStream * const stream)
{
    static_cast <VideoStream*> (stream)->unref ();
}

void moment_remove_stream (MomentStreamKey const stream_key)
{
    if (!stream_key)
	return;

    MomentServer * const moment = MomentServer::getInstance ();
    moment->removeVideoStream (MomentServer::VideoStreamKey::fromVoidPtr (stream_key));
}

namespace {
class MomentStreamHandlerWrapper : public Referenced
{
public:
    MomentStreamHandler stream_handler;
    GenericInformer::SubscriptionKey subscription_key;

    // TODO Make use of cb_mutex
    Mutex cb_mutex;
};

void stream_rtmpCommandMessage (RtmpConnection * const mt_nonnull conn,
				VideoStream::MessageInfo * const mt_nonnull msg_info,
				ConstMemory const &method_name,
				AmfDecoder     * const mt_nonnull amf_decoder,
				void           * const _stream_handler)
{
    MomentStreamHandler * const stream_handler = static_cast <MomentStreamHandler*> (_stream_handler);

    MomentMessage msg;

//    PagePool::PageListArray pl_array (/* TODO */);

    if (stream_handler->command_cb)
	stream_handler->command_cb (&msg, stream_handler->command_cb_data);
}

void stream_closed (void * const _stream_handler)
{
    MomentStreamHandler * const stream_handler = static_cast <MomentStreamHandler*> (_stream_handler);

    if (stream_handler->closed_cb)
	stream_handler->closed_cb (stream_handler->closed_cb_data);
}

VideoStream::EventHandler stream_event_handler = {
    NULL /* audioMessage */,
    NULL /* videoMessage */,
    stream_rtmpCommandMessage,
    stream_closed
};
} // namespace {}

MomentStreamHandlerKey moment_stream_add_handler (MomentStream        * const stream,
						  MomentStreamHandler * const stream_handler)
{
    Ref<MomentStreamHandlerWrapper> wrapper = grab (new MomentStreamHandlerWrapper);
    wrapper->stream_handler = *stream_handler;
    wrapper->subscription_key =
	    static_cast <VideoStream*> (stream)->getEventInformer()->subscribe (
		    &stream_event_handler, wrapper, wrapper /* ref_data */, NULL);
    return static_cast <void*> (wrapper);
}

void moment_stream_remove_handler (MomentStream           * const stream,
				   MomentClientHandlerKey   const stream_handler_key)
{
    MomentStreamHandlerWrapper * const wrapper =
	    static_cast <MomentStreamHandlerWrapper*> (stream_handler_key);
    static_cast <VideoStream*> (stream)->getEventInformer()->unsubscribe (wrapper->subscription_key);
}


// _______________________________ Client events _______________________________

struct MomentClientHandler {
    MomentClientConnectedCallback connected_cb;
    void *connected_cb_data;

    MomentClientDisconnectedCallback disconnected_cb;
    void *disconnected_cb_data;

    MomentStartWatchingCallback start_watching_cb;
    void *start_watching_cb_data;

    MomentStartStreamingCallback start_streaming_cb;
    void *start_streaming_cb_data;
};

MomentClientHandler* moment_client_handler_new ()
{
    MomentClientHandler * const client_handler = new MomentClientHandler;
    assert (client_handler);

    client_handler->connected_cb = NULL;
    client_handler->connected_cb_data = NULL;

    client_handler->disconnected_cb = NULL;
    client_handler->disconnected_cb_data = NULL;

    client_handler->start_watching_cb = NULL;
    client_handler->start_watching_cb_data = NULL;

    client_handler->start_streaming_cb = NULL;
    client_handler->start_streaming_cb_data = NULL;

    return client_handler;
}

void moment_client_handler_delete (MomentClientHandler *client_handler)
{
    delete client_handler;
}

void moment_client_handler_set_connected (MomentClientHandler           * const client_handler,
					  MomentClientConnectedCallback   const cb,
					  void                          * const user_data)
{
    client_handler->connected_cb = cb;
    client_handler->connected_cb_data = user_data;
}

void moment_client_handler_set_disconnected (MomentClientHandler              * const client_handler,
					     MomentClientDisconnectedCallback   const cb,
					     void                             * const user_data)
{
    client_handler->disconnected_cb = cb;
    client_handler->disconnected_cb_data = user_data;
}

void moment_client_handler_set_start_watching (MomentClientHandler         * const client_handler,
					       MomentStartWatchingCallback   const cb,
					       void                        * const user_data)
{
    client_handler->start_watching_cb = cb;
    client_handler->start_watching_cb_data = user_data;
}

void moment_client_handler_set_start_streaming (MomentClientHandler          * const client_handler,
						MomentStartStreamingCallback   const cb,
						void                         * const user_data)
{
    client_handler->start_streaming_cb = cb;
    client_handler->start_streaming_cb_data = user_data;
}

namespace {
class Api_ClientHandler_Wrapper : public Referenced
{
public:
    MomentClientHandler ext_client_handler;
    MomentServer::ClientHandlerKey int_client_handler_key;
};
} // namespace {}

class MomentClientSession : public Object
{
public:
    Ref<MomentServer::ClientSession> int_client_session;
    Ref<Api_ClientHandler_Wrapper> api_client_handler_wrapper;
    void *client_cb_data;

    MomentClientSession ()
    {
	logD_ (_func, "0x", fmt_hex, (UintPtr) this);
    }

    ~MomentClientSession ()
    {
	logD_ (_func, "0x", fmt_hex, (UintPtr) this);
    }
};

void moment_client_session_ref (MomentClientSession * const api_client_session)
{
    api_client_session->ref ();
}

void moment_client_session_unref (MomentClientSession * const api_client_session)
{
    api_client_session->unref ();
}

void moment_client_session_disconnect (MomentClientSession * const api_client_session)
{
    MomentServer * const moment = MomentServer::getInstance ();
    moment->disconnect (api_client_session->int_client_session);
}

namespace {
void client_rtmpCommandMessage (RtmpConnection           * const mt_nonnull conn,
				VideoStream::MessageInfo * const mt_nonnull msg_info,
				ConstMemory const        &method_name,
				AmfDecoder               * const mt_nonnull amf_decoder,
				void                     * const _api_client_session)
{
    MomentClientSession * const api_client_session = static_cast <MomentClientSession*> (_api_client_session);

  // TODO
}

void client_clientDisconnected (void * const _api_client_session)
{
    MomentClientSession * const api_client_session = static_cast <MomentClientSession*> (_api_client_session);

    logD_ (_func, "api_client_session: 0x", fmt_hex, (UintPtr) api_client_session);
    logD_ (_func, "api_client_session refcount before: ", api_client_session->getRefCount ());

    if (api_client_session->api_client_handler_wrapper->ext_client_handler.disconnected_cb) {
	api_client_session->api_client_handler_wrapper->ext_client_handler.disconnected_cb (
		api_client_session->client_cb_data,
		api_client_session->api_client_handler_wrapper->ext_client_handler.disconnected_cb_data);
    }

    logD_ (_func, "api_client_session refcount after: ", api_client_session->getRefCount ());
    api_client_session->unref ();
}

MomentServer::ClientSession::Events client_session_events = {
    client_rtmpCommandMessage,
    client_clientDisconnected
};

Ref<VideoStream> client_startWatching (ConstMemory   const stream_name,
				       void        * const _api_client_session)
{
    MomentClientSession * const api_client_session = static_cast <MomentClientSession*> (_api_client_session);

    if (api_client_session->api_client_handler_wrapper->ext_client_handler.start_watching_cb) {
	MomentStream * const ext_stream =
		api_client_session->api_client_handler_wrapper->ext_client_handler.start_watching_cb (
			(char const *) stream_name.mem(),
			stream_name.len(),
			api_client_session->client_cb_data,
			api_client_session->api_client_handler_wrapper->ext_client_handler.start_watching_cb_data);

	return static_cast <VideoStream*> (ext_stream);
    }

    return NULL;
}

Ref<VideoStream> client_startStreaming (ConstMemory   const stream_name,
					void        * const _api_client_session)
{
    MomentClientSession * const api_client_session = static_cast <MomentClientSession*> (_api_client_session);

    if (api_client_session->api_client_handler_wrapper->ext_client_handler.start_streaming_cb) {
	MomentStream * const ext_stream =
		api_client_session->api_client_handler_wrapper->ext_client_handler.start_streaming_cb (
			(char const *) stream_name.mem(),
			stream_name.len(),
			api_client_session->client_cb_data,
			api_client_session->api_client_handler_wrapper->ext_client_handler.start_streaming_cb_data);

	return static_cast <VideoStream*> (ext_stream);
    }

    return NULL;
}

MomentServer::ClientSession::Backend client_session_backend = {
    client_startWatching,
    client_startStreaming
};

void client_clientConnected (MomentServer::ClientSession * const int_client_session,
			     ConstMemory const &app_name,
			     ConstMemory const &full_app_name,
			     void * const _api_client_handler_wrapper)
{
    Api_ClientHandler_Wrapper * const api_client_handler_wrapper =
	    static_cast <Api_ClientHandler_Wrapper*> (_api_client_handler_wrapper);
    MomentClientHandler * const ext_client_handler = &api_client_handler_wrapper->ext_client_handler;

    Ref<MomentClientSession> api_client_session = grab (new MomentClientSession);
    api_client_session->int_client_session = int_client_session;
    api_client_session->client_cb_data = NULL;
    api_client_session->api_client_handler_wrapper = api_client_handler_wrapper;

    if (ext_client_handler->connected_cb) {
	ext_client_handler->connected_cb (api_client_session,
					  (char const *) app_name.mem(),
					  app_name.len(),
					  (char const *) full_app_name.mem(),
					  full_app_name.len(),
					  &api_client_session->client_cb_data,
					  ext_client_handler->connected_cb_data);
    }

    if (!int_client_session->isConnected_subscribe (
		CbDesc<MomentServer::ClientSession::Events> (
			&client_session_events, api_client_session, api_client_session)))
    {
	if (ext_client_handler->disconnected_cb) {
	    ext_client_handler->disconnected_cb (api_client_session->client_cb_data,
						 ext_client_handler->disconnected_cb_data);
	}
    }

    int_client_session->setBackend (
	    CbDesc<MomentServer::ClientSession::Backend> (
		    &client_session_backend, api_client_session, api_client_session));

    api_client_session.setNoUnref ((MomentClientSession*) NULL);
}

MomentServer::ClientHandler api_client_handler = {
    client_clientConnected
};
} // namespace {}

MomentClientHandlerKey moment_add_client_handler (MomentClientHandler * const ext_client_handler,
						  char const          * const prefix_buf,
						  size_t                const prefix_len)
{
    MomentServer * const moment = MomentServer::getInstance ();

    Ref<Api_ClientHandler_Wrapper> api_client_handler_wrapper = grab (new Api_ClientHandler_Wrapper);
    api_client_handler_wrapper->ext_client_handler = *ext_client_handler;
    api_client_handler_wrapper->int_client_handler_key =
	    moment->addClientHandler (
		    CbDesc<MomentServer::ClientHandler> (&api_client_handler,
							 api_client_handler_wrapper,
							 NULL /* coderef_container */,
							 api_client_handler_wrapper),
		    ConstMemory (prefix_buf, prefix_len));

    return static_cast <MomentClientHandlerKey> (api_client_handler_wrapper);
}

void moment_remove_client_handler (MomentClientHandlerKey const ext_client_handler_key)
{
    Api_ClientHandler_Wrapper * const api_client_handler_wrapper =
	    static_cast <Api_ClientHandler_Wrapper*> (ext_client_handler_key);

    MomentServer * const moment = MomentServer::getInstance ();

    moment->removeClientHandler (api_client_handler_wrapper->int_client_handler_key);
}

void moment_client_send_command_message (MomentClientSession * const api_client_session,
					 unsigned char const * const msg_buf,
					 size_t                const msg_len)
{
    CodeRef conn_ref;
    RtmpConnection * const conn = api_client_session->int_client_session->getRtmpConnection (&conn_ref);
    if (conn)
	conn->sendCommandMessage_AMF0 (RtmpConnection::DefaultMessageStreamId, ConstMemory (msg_buf, msg_len));
}


// _____________________________ Config file access ____________________________

MomentConfigIterator moment_config_section_iter_begin (MomentConfigSection * const ext_section)
{
    MConfig::Section *int_section = static_cast <MConfig::Section*> (ext_section);
    if (!int_section) {
	MomentServer * const moment = MomentServer::getInstance ();
	int_section = moment->getConfig()->getRootSection();
    }

    return MConfig::Section::iter (*int_section).getAsVoidPtr();
}

MomentConfigSectionEntry* moment_confg_section_iter_next (MomentConfigSection  * const ext_section,
							  MomentConfigIterator   const ext_iter)
{
    MConfig::Section * const int_section = static_cast <MConfig::Section*> (ext_section);
    MConfig::Section::iter int_iter = MConfig::Section::iter::fromVoidPtr (ext_iter);
    return static_cast <MomentConfigSectionEntry*> (int_section->iter_next (int_iter));
}

MomentConfigSection* moment_config_section_entry_is_section (MomentConfigSectionEntry * const ext_section_entry)
{
    MConfig::SectionEntry * const int_section_entry = static_cast <MConfig::SectionEntry*> (ext_section_entry);
    if (int_section_entry->getType() == MConfig::SectionEntry::Type_Section)
	return static_cast <MomentConfigSection*> (static_cast <MConfig::Section*> (int_section_entry));

    return NULL;
}

MomentConfigOption* moment_config_section_entry_is_option (MomentConfigSectionEntry * const ext_section_entry)
{
    MConfig::SectionEntry * const int_section_entry = static_cast <MConfig::SectionEntry*> (ext_section_entry);
    if (int_section_entry->getType() == MConfig::SectionEntry::Type_Option)
	return static_cast <MomentConfigOption*> (static_cast <MConfig::Option*> (int_section_entry));

    return NULL;
}

size_t moment_config_option_get_value (MomentConfigOption * const ext_option,
				       char               * const buf,
				       size_t               const len)
{
    MConfig::Option * const int_option = static_cast <MConfig::Option*> (ext_option);
    MConfig::Value * const val = int_option->getValue ();
    if (!val)
	return 0;

    Ref<String> const str = val->getAsString ();
    Size tocopy = str->mem().len();
    if (tocopy > len)
	tocopy = len;

    memcpy (buf, str->mem().mem(), tocopy);
    return str->mem().len();
}

size_t moment_config_get_option (char   * const opt_path,
				 size_t   const opt_path_len,
				 char   * const buf,
				 size_t   const len,
				 bool   * const ret_is_set)
{
    MomentServer * const moment = MomentServer::getInstance ();
    MConfig::Option * const int_option =
	    moment->getConfig()->getOption (ConstMemory (opt_path, opt_path_len), false /* create */);
    if (!int_option) {
	if (ret_is_set)
	    *ret_is_set = false;

	return 0;
    }

    if (ret_is_set)
	*ret_is_set = true;

    return moment_config_option_get_value (static_cast <MomentConfigOption*> (int_option),
					   buf, len);
}


// __________________________________ Logging __________________________________

void moment_log (MomentLogLevel   const log_level,
		 char const     * const fmt,
		 ...)
{
    va_list ap;
    va_start (ap, fmt);
    moment_vlog (log_level, fmt, ap);
    va_end (ap);
}

void moment_vlog (MomentLogLevel   const log_level,
		  char const     * const fmt,
		  va_list                orig_ap)
{
    int n;
    int size = 256;

    char *p;
    va_list ap;

    while (1) {
	p = new char [size];

	va_copy (ap, orig_ap);
	n = vsnprintf (p, size, fmt, ap);
	va_end (ap);

	if (n > -1 && n < size)
	    break;

	if (n > -1)
	    size = n + 1;
	else
	    size *= 2;

	delete p;
    }

    log_ ((LogLevel::Value) log_level, ConstMemory (p, n));
}

void moment_log_d (char const *fmt, ...)
{
    va_list ap;
    va_start (ap, fmt);
    moment_vlog (MomentLogLevel_D, fmt, ap);
    va_end (ap);
}

void moment_log_i (char const *fmt, ...)
{
    va_list ap;
    va_start (ap, fmt);
    moment_vlog (MomentLogLevel_I, fmt, ap);
    va_end (ap);
}

void moment_log_w (char const *fmt, ...)
{
    va_list ap;
    va_start (ap, fmt);
    moment_vlog (MomentLogLevel_W, fmt, ap);
    va_end (ap);
}

void moment_log_e (char const *fmt, ...)
{
    va_list ap;
    va_start (ap, fmt);
    moment_vlog (MomentLogLevel_E, fmt, ap);
    va_end (ap);
}

void moment_log_h (char const *fmt, ...)
{
    va_list ap;
    va_start (ap, fmt);
    moment_vlog (MomentLogLevel_H, fmt, ap);
    va_end (ap);
}

void moment_log_f (char const *fmt, ...)
{
    va_list ap;
    va_start (ap, fmt);
    moment_vlog (MomentLogLevel_F, fmt, ap);
    va_end (ap);
}


} // extern "C"

