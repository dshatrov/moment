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


#include <libmary/module_init.h>

#include <moment/libmoment.h>


namespace Moment {

namespace {

RtmpService rtmp_service (NULL);
RtmptService rtmpt_service (NULL);

class ClientSession : public Object
{
public:
    mt_mutex (mutex) bool valid;

    mt_const RtmpConnection *rtmp_conn;
    // Remember that RtmpConnection must be available when we're calling
    // RtmpServer's methods. We must take special care to ensure that this
    // holds. See takeRtmpConnRef().
    RtmpServer rtmp_server;

    // Synchronized by rtmp_server.
    bool watching;

    // Returns 'false' if ClientSession is invalid already.
    bool invalidate ()
    {
      StateMutexLock l (&mutex);
        bool const ret_valid = valid;
	valid = false;
	return ret_valid;
    }

    // Secures a reference to rtmp_conn so that it is safe to call rtmp_server's
    // methods.
    void takeRtmpConnRef (Ref<Object> * const mt_nonnull ret_ref)
    {
	mutex.lock ();

	if (valid)
	    *ret_ref = rtmp_conn->getCoderefContainer();
	else
	    *ret_ref = NULL;

	mutex.unlock ();
    }

    ClientSession ()
	: valid (true),
	  watching (false)
    {
    }

#if 0
    ~ClientSession ()
    {
	logD_ (_func, "0x", fmt_hex, (UintPtr) this);
    }
#endif
};

void streamAudioMessage (VideoStream::MessageInfo * const mt_nonnull msg_info,
			 PagePool::PageListHead   * const mt_nonnull page_list,
			 Size                       const msg_len,
			 void                     * const _session)
{
//    logD_ (_func_);

    ClientSession * const client_session = static_cast <ClientSession*> (_session);

    Ref<Object> rtmp_conn_ref;
    client_session->takeRtmpConnRef (&rtmp_conn_ref);
    if (!rtmp_conn_ref)
	return;

    RtmpConnection::MessageInfo rtmp_msg_info;
    rtmp_msg_info.msg_stream_id = RtmpConnection::DefaultMessageStreamId;
    rtmp_msg_info.timestamp = (Uint32) msg_info->timestamp;

    // TODO Prehunking logics must match MomentVideo arch.
    client_session->rtmp_server.sendAudioMessage (&rtmp_msg_info, page_list, msg_len, false /* prechunked */);
}

void streamVideoMessage (VideoStream::MessageInfo * const mt_nonnull msg_info,
			 PagePool::PageListHead   * const mt_nonnull page_list,
			 Size                       const msg_len,
			 void                     * const _session)
{
//    logD_ (_func_);

    ClientSession * const client_session = static_cast <ClientSession*> (_session);

    Ref<Object> rtmp_conn_ref;
    client_session->takeRtmpConnRef (&rtmp_conn_ref);
    if (!rtmp_conn_ref)
	return;

    RtmpConnection::MessageInfo rtmp_msg_info;
    rtmp_msg_info.msg_stream_id = RtmpConnection::DefaultMessageStreamId;
    rtmp_msg_info.timestamp = (Uint32) msg_info->timestamp;

    // TODO Prehunking logics must match MomentVideo arch.
    client_session->rtmp_server.sendVideoMessage (&rtmp_msg_info, page_list, msg_len, false /* prechunked */);
}

void streamClosed (void * const /* _session */)
{
    logD_ (_func_);
}

VideoStream::EventHandler /* TODO Allow consts in Informer_ */ /* const */ video_event_handler = {
    streamAudioMessage,
    streamVideoMessage,
    NULL /* rtmpCommandMessage */,
    streamClosed
};

Result startWatching (ConstMemory const &stream_name,
		      void * const _client_session)
{
    // TODO class MomentRtmpModule
    MomentServer * const moment = MomentServer::getInstance();

    logD_ (_func, "client_session 0x", fmt_hex, (UintPtr) _client_session);
    logD_ (_func, "stream_name: ", stream_name);

    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    // TODO Direct video stream to the client. (Subscribe to that video stream's events.
    //      Some kind of flow control is needed.

    Ref<VideoStream> const video_stream = moment->getVideoStream (stream_name);
    if (!video_stream) {
	logE_ (_func, "video stream not found: ", stream_name);
	return Result::Failure;
    }

    if (client_session->watching) {
	logE_ (_func, "already watching another stream");
	return Result::Success;
    }
    client_session->watching = true;

    video_stream->getEventInformer()->subscribe (&video_event_handler, client_session, NULL /* ref_data */, client_session);

    return Result::Success;
}

RtmpServer::Frontend const rtmp_server_frontend = {
    NULL /* startStreaming */, // TODO Accept arbitrary incoming streams, if enabled.
    startWatching,
    NULL /* commandMessage */
};

Result commandMessage (RtmpConnection::MessageInfo * const mt_nonnull msg_info,
		       PagePool::PageListHead * const mt_nonnull page_list,
		       Size                     const msg_len,
		       AmfEncoding              const amf_encoding,
		       void                   * const _client_session)
{
    logD_ (_func_);

    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);
    // No need to call takeRtmpConnRef(), because this it rtmp_conn's callback.
    return client_session->rtmp_server.commandMessage (msg_info, page_list, msg_len, amf_encoding);
}

void closed (Exception * const exc,
	     void      * const _client_session)
{
    logD_ (_func, "client_session 0x", fmt_hex, (UintPtr) _client_session);

    if (exc)
	logE_ (_func, exc->toString());

    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);
    if (!client_session->invalidate())
	return;

    client_session->unref ();
}

RtmpConnection::Frontend const rtmp_frontend = {
    NULL /* handshakeComplete */,
    commandMessage,
    NULL /* audioMessage */,
    NULL /* videoMessage */,
    closed
};

Result clientConnected (RtmpConnection * mt_nonnull const rtmp_conn,
			void * const /* cb_data */)
{
    logD_ (_func_);

    Ref<ClientSession> const client_session = grab (new ClientSession);
    client_session->rtmp_conn = rtmp_conn;

    client_session->rtmp_server.setFrontend (Cb<RtmpServer::Frontend> (
	    &rtmp_server_frontend, client_session, client_session));
    client_session->rtmp_server.setRtmpConnection (rtmp_conn);

    rtmp_conn->setFrontend (Cb<RtmpConnection::Frontend> (
	    &rtmp_frontend, client_session, client_session));

    rtmp_conn->startServer ();

    client_session->ref ();

    return Result::Success;
}

RtmpVideoService::Frontend const rtmp_video_service_frontend = {
    clientConnected
};

void momentRtmptInit ()
{
    MomentServer * const moment = MomentServer::getInstance();
    ServerApp * const server_app = moment->getServerApp();
    MConfig::Config * const config = moment->getConfig();

    {
	ConstMemory const opt_name = "mod_rtmp/enable";
	MConfig::Config::BooleanValue const enable = config->getBoolean (opt_name);
	if (enable == MConfig::Config::Boolean_Invalid) {
	    logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
	    return;
	}

	if (enable != MConfig::Config::Boolean_True) {
	    logI_ (_func, "Unrestricted RTMP access module is not enabled. "
		   "Set \"mod_rtmp/enable\" option to \"y\" to enable.");
	    return;
	}
    }

    {
	rtmp_service.setFrontend (Cb<RtmpVideoService::Frontend> (
		&rtmp_video_service_frontend, NULL, NULL));

	rtmp_service.setTimers (server_app->getTimers());
	rtmp_service.setPollGroup (server_app->getPollGroup());
	rtmp_service.setPagePool (moment->getPagePool());

	if (!rtmp_service.init()) {
	    logE_ (_func, "rtmp_service.init() failed: ", exc->toString());
	    return;
	}

	IpAddress addr;
	{
	    ConstMemory rtmp_bind = config->getString ("mod_rtmp/rtmp_bind");
	    logD_ (_func, "rtmp_bind: ", rtmp_bind);
	    if (!rtmp_bind.isNull ()) {
		if (!setIpAddress (rtmp_bind, &addr)) {
		    logE_ (_func, "setIpAddress() failed (rtmp)");
		    return;
		}

		if (!rtmp_service.bind (addr)) {
		    logE_ (_func, "rtmp_service.bind() faled: ", exc->toString());
		    return;
		}

		if (!rtmp_service.start ()) {
		    logE_ (_func, "rtmp_service.start() failed: ", exc->toString());
		    return;
		}
	    }
	}
    }

    {
	rtmpt_service.setFrontend (Cb<RtmpVideoService::Frontend> (
		&rtmp_video_service_frontend, NULL, NULL));

	rtmpt_service.setTimers (server_app->getTimers());
	rtmpt_service.setPollGroup (server_app->getPollGroup());
	rtmpt_service.setPagePool (moment->getPagePool());

	if (!rtmpt_service.init()) {
	    logE_ (_func, "rtmpt_service.init() failed: ", exc->toString());
	    return;
	}

	IpAddress addr;
	{
	    ConstMemory rtmpt_bind = config->getString ("mod_rtmp/rtmpt_bind");
	    logD_ (_func, "rtmpt_bind: ", rtmpt_bind);
	    if (!rtmpt_bind.isNull ()) {
		if (!setIpAddress (rtmpt_bind, &addr)) {
		    logE_ (_func, "setIpAddress() failed (rtmp)");
		    return;
		}

		if (!rtmpt_service.bind (addr)) {
		    logE_ (_func, "rtmpt_service.bind() faled: ", exc->toString());
		    return;
		}

		if (!rtmpt_service.start ()) {
		    logE_ (_func, "rtmpt_service.start() failed: ", exc->toString());
		    return;
		}
	    }
	}
    }
}

void momentRtmptUnload ()
{
}

}

}


namespace M {

void libMary_moduleInit ()
{
    logD_ ("RTMP MODULE INIT");

    Moment::momentRtmptInit ();
}

void libMary_moduleUnload()
{
    logD_ ("RTMP MODULE UNLOAD");

    Moment::momentRtmptUnload ();
}

}

