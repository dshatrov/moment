/*  Moment Video Server - High performance media server
    Copyright (C) 2011 Dmitry Shatrov
    e-mail: shatrov@gmail.com

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <libmary/module_init.h>

#include <moment/libmoment.h>


#define MOMENT_RTMP__WAIT_FOR_KEYFRAME
#define MOMENT_RTMP__AUDIO_WAITS_VIDEO
#define MOMENT_RTMP__FLOW_CONTROL


namespace Moment {

namespace {

namespace {
LogGroup libMary_logGroup_mod_rtmp ("mod_rtmp", LogLevel::E);
LogGroup libMary_logGroup_framedrop ("mod_rtmp_framedrop", LogLevel::D);
}

RtmpService rtmp_service (NULL);
RtmptService rtmpt_service (NULL);

bool audio_waits_video = false;

#ifdef MOMENT_RTMP__WAIT_FOR_KEYFRAME
Count no_keyframe_limit = 250; // 25 fps * 10 seconds
#endif

class ClientSession : public Object
{
public:
    mt_mutex (mutex) bool valid;

    mt_const RtmpConnection *rtmp_conn;
    // Remember that RtmpConnection must be available when we're calling
    // RtmpServer's methods. We must take special care to ensure that this
    // holds. See takeRtmpConnRef().
    RtmpServer rtmp_server;

    mt_mutex (mutex) Ref<MomentServer::ClientSession> srv_session;

    mt_mutex (mutex) Ref<VideoStream> video_stream;
    // TODO Deprecated field
    mt_mutex (mutex) MomentServer::VideoStreamKey video_stream_key;

#ifdef MOMENT_RTMP__FLOW_CONTROL
    mt_mutex (mutex) bool overloaded;
#endif

#ifdef MOMENT_RTMP__WAIT_FOR_KEYFRAME
    // Used from streamVideoMessage() only.
    mt_mutex (mutex) Count no_keyframe_counter;
    mt_mutex (mutex) bool keyframe_sent;
    mt_mutex (mutex) bool first_keyframe_sent;
#endif

    // Synchronized by rtmp_server.
    bool watching;

#if 0
    // Returns 'false' if ClientSession is invalid already.
    bool invalidate ()
    {
      StateMutexLock l (&mutex);
        bool const ret_valid = valid;
	valid = false;
	return ret_valid;
    }
#endif

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
#ifdef MOMENT_RTMP__FLOW_CONTROL
	  overloaded (false),
#endif
#ifdef MOMENT_RTMP__WAIT_FOR_KEYFRAME
	  no_keyframe_counter (0),
	  keyframe_sent (false),
	  first_keyframe_sent (false),
#endif
	  watching (false)
    {
	logD_ (_func, "0x", fmt_hex, (UintPtr) this);
    }

    ~ClientSession ()
    {
	logD_ (_func, "0x", fmt_hex, (UintPtr) this);
    }
};

void destroyClientSession (ClientSession * const client_session)
{
    client_session->mutex.lock ();

    if (!client_session->valid) {
	client_session->mutex.unlock ();
	logD (mod_rtmp, _func, "invalid session");
	return;
    }
    client_session->valid = false;

    Ref<VideoStream> const video_stream = client_session->video_stream;
    MomentServer::VideoStreamKey const video_stream_key = client_session->video_stream_key;

    Ref<MomentServer::ClientSession> const srv_session = client_session->srv_session;
    client_session->srv_session = NULL;

    client_session->mutex.unlock ();

    // TODO class MomentRtmpModule
    MomentServer * const moment = MomentServer::getInstance();

    if (srv_session)
	moment->clientDisconnected (srv_session);

    // Closing video stream *after* firing clientDisconnected() to avoid
    // premature closing of client connections in streamClosed().
    if (video_stream)
	video_stream->close ();

    if (video_stream_key)
	moment->removeVideoStream (video_stream_key);

    client_session->unref ();
}

void streamAudioMessage (VideoStream::AudioMessage * const mt_nonnull msg,
			 void                      * const _session)
{
//    logD_ (_func_);

    ClientSession * const client_session = static_cast <ClientSession*> (_session);

    Ref<Object> rtmp_conn_ref;
    client_session->takeRtmpConnRef (&rtmp_conn_ref);
    if (!rtmp_conn_ref)
	return;

    client_session->mutex.lock ();

#ifdef MOMENT_RTMP__AUDIO_WAITS_VIDEO
    if (audio_waits_video
	&& msg->frame_type == VideoStream::AudioFrameType::RawData)
    {
	if (!client_session->first_keyframe_sent) {
	    client_session->mutex.unlock ();
	    return;
	}
    }
#endif

#ifdef MOMENT_RTMP__FLOW_CONTROL
    if (client_session->overloaded
	&& msg->frame_type == VideoStream::AudioFrameType::RawData)
    {
      // Connection overloaded, dropping this audio frame.
	logD (framedrop, _func, "Connection overloaded, dropping audio frame");
	client_session->mutex.unlock ();
	return;
    }
#endif

    client_session->mutex.unlock ();

    client_session->rtmp_server.sendAudioMessage (msg);
}

void streamVideoMessage (VideoStream::VideoMessage * const mt_nonnull msg,
			 void                      * const _session)
{
//    logD_ (_func_);

    ClientSession * const client_session = static_cast <ClientSession*> (_session);

    Ref<Object> rtmp_conn_ref;
    client_session->takeRtmpConnRef (&rtmp_conn_ref);
    if (!rtmp_conn_ref)
	return;

    client_session->mutex.lock ();

#ifdef MOMENT_RTMP__FLOW_CONTROL
    if (client_session->overloaded
	&& (   msg->frame_type == VideoStream::VideoFrameType::KeyFrame
	    || msg->frame_type == VideoStream::VideoFrameType::InterFrame
	    || msg->frame_type == VideoStream::VideoFrameType::DisposableInterFrame
	    || msg->frame_type == VideoStream::VideoFrameType::GeneratedKeyFrame))
    {
      // Connection overloaded, dropping this video frame. In general, we'll
      // have to wait for the next keyframe after we've dropped a frame.
      // We do not care about disposable frames yet.

	logD (framedrop, _func, "Connection overloaded, dropping video frame");

	// TEST
//	logs->print (".");
//	logs->flush ();

#ifdef MOMENT_RTMP__WAIT_FOR_KEYFRAME
	client_session->no_keyframe_counter = 0;
	client_session->keyframe_sent = false;
#endif

	client_session->mutex.unlock ();
	return;
    }
#endif // MOMENT_RTMP__FLOW_CONTROL

#ifdef MOMENT_RTMP__WAIT_FOR_KEYFRAME
    bool got_keyframe = false;
    if (msg->frame_type == VideoStream::VideoFrameType::KeyFrame ||
	msg->frame_type == VideoStream::VideoFrameType::GeneratedKeyFrame)
    {
	got_keyframe = true;
    } else
    if (!client_session->keyframe_sent
	&& (   msg->frame_type == VideoStream::VideoFrameType::InterFrame
	    || msg->frame_type == VideoStream::VideoFrameType::DisposableInterFrame))
    {
	++client_session->no_keyframe_counter;
	if (client_session->no_keyframe_counter >= no_keyframe_limit) {
	    got_keyframe = true;
	} else {
	  // Waiting for a keyframe, dropping current video frame.
	    client_session->mutex.unlock ();
	    return;
	}
    }

    if (got_keyframe) {
	client_session->no_keyframe_counter = 0;
	client_session->keyframe_sent = true;
	client_session->first_keyframe_sent = true;
    }
#endif

    client_session->mutex.unlock ();

//    logD_ (_func, "sending ", toString (msg->codec_id), ", ", toString (msgo->frame_type));

    client_session->rtmp_server.sendVideoMessage (msg);
}

void streamClosed (void * const _session)
{
    logD_ (_func_);
    ClientSession * const client_session = static_cast <ClientSession*> (_session);
// Unnecessary    destroyClientSession (client_session);
    client_session->rtmp_conn->closeAfterFlush ();
}

VideoStream::EventHandler /* TODO Allow consts in Informer_ */ /* const */ video_event_handler = {
    streamAudioMessage,
    streamVideoMessage,
    NULL /* rtmpCommandMessage */,
    streamClosed
};

Result connect (ConstMemory const &app_name,
		void * const _client_session)
{
    logD_ (_func, "app_name: ", app_name);

    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    // TODO class MomentRtmpModule
    MomentServer * const moment = MomentServer::getInstance();

    Ref<MomentServer::ClientSession> const srv_session = moment->rtmpClientConnected (app_name, client_session->rtmp_conn);
    if (!srv_session)
	return Result::Failure;

    client_session->mutex.lock ();
    if (!client_session->valid) {
	assert (!client_session->srv_session);
	client_session->mutex.unlock ();
	return Result::Failure;
    }
    client_session->srv_session = srv_session;
    client_session->mutex.unlock ();

    return Result::Success;
}

Result startStreaming (ConstMemory const &_stream_name,
		       void * const _client_session)
{
    logD_ (_func, "stream_name: ", _stream_name);

    ConstMemory stream_name = _stream_name;
    {
	Byte const * const name_sep = (Byte const *) memchr (stream_name.mem(), '?', stream_name.len());
	if (name_sep)
	    stream_name = stream_name.region (0, name_sep - stream_name.mem());
    }

    // TODO class MomentRtmpModule
    MomentServer * const moment = MomentServer::getInstance();

    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    // 'srv_session' is created in connect(), which is synchronized with
    // startStreaming(). No locking needed.
    Ref<VideoStream> const video_stream = moment->startStreaming (client_session->srv_session, stream_name);
    if (!video_stream)
	return Result::Failure;

    client_session->mutex.lock ();
    client_session->video_stream = video_stream;
    client_session->mutex.unlock ();

#if 0
// Deprecated
    MomentServer::VideoStreamKey const video_stream_key =
	    moment->addVideoStream (client_session->video_stream, stream_name);

    client_session->mutex.lock ();
    client_session->video_stream_key = video_stream_key;
    client_session->mutex.unlock ();
#endif

    return Result::Success;
}

Result startWatching (ConstMemory const &stream_name,
		      void * const _client_session)
{
    // TODO class MomentRtmpModule
    MomentServer * const moment = MomentServer::getInstance();

    logD (mod_rtmp, _func, "client_session 0x", fmt_hex, (UintPtr) _client_session);
    logD (mod_rtmp, _func, "stream_name: ", stream_name);

    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    if (client_session->watching) {
	logE (mod_rtmp, _func, "already watching another stream");
	return Result::Success;
    }
    client_session->watching = true;

    Ref<VideoStream> const video_stream = moment->startWatching (client_session->srv_session, stream_name);
#if 0
// Deprecated
    Ref<VideoStream> const video_stream = moment->getVideoStream (stream_name);
#endif
    if (!video_stream) {
	logE (mod_rtmp, _func, "video stream not found: ", stream_name);
	return Result::Failure;
    }

    video_stream->lock ();
    client_session->rtmp_server.sendInitialMessages_unlocked (video_stream->getFrameSaver());
    video_stream->getEventInformer()->subscribe_unlocked (&video_event_handler, client_session, NULL /* ref_data */, client_session);
    video_stream->unlock ();

    return Result::Success;
}

RtmpServer::CommandResult server_commandMessage (RtmpConnection       * const mt_nonnull conn,
						 Uint32                 const msg_stream_id,
						 ConstMemory const    &method_name,
						 VideoStream::Message * const mt_nonnull msg,
						 AmfDecoder           * const mt_nonnull amf_decoder,
						 void                 * const _client_session)
{
    logD_ (_func, "method_name: ", method_name);

    MomentServer * const moment = MomentServer::getInstance();
    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    client_session->mutex.lock ();
    Ref<MomentServer::ClientSession> const srv_session = client_session->srv_session;
    client_session->mutex.unlock ();

    if (!srv_session) {
	logW_ (_func, "No server session, command message dropped");
	return RtmpServer::CommandResult::UnknownCommand;
    }

    moment->rtmpCommandMessage (srv_session, conn, /* TODO Unnecessary? msg_stream_id, */ msg, method_name, amf_decoder);

    return RtmpServer::CommandResult::Success;
}

RtmpServer::Frontend const rtmp_server_frontend = {
    connect,
    startStreaming /* startStreaming */,
    startWatching,
    server_commandMessage
};

Result audioMessage (VideoStream::AudioMessage * const mt_nonnull msg,
		     void                      * const _client_session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    // We create 'video_stream' in startStreaming()/startWatching(), which is
    // synchronized with autioMessage(). No locking needed.
    if (client_session->video_stream)
	client_session->video_stream->fireAudioMessage (msg);

    return Result::Success;
}

Result videoMessage (VideoStream::VideoMessage * const mt_nonnull msg,
		     void                      * const _client_session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    // We create 'video_stream' in startStreaming()/startWatching(), which is
    // synchronized with videoMessage(). No locking needed.
    if (client_session->video_stream)
	client_session->video_stream->fireVideoMessage (msg);

    return Result::Success;
}

Result commandMessage (VideoStream::Message * const mt_nonnull msg,
		       Uint32                 const msg_stream_id,
		       AmfEncoding            const amf_encoding,
		       void                 * const _client_session)
{
    logD (mod_rtmp, _func_);

    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);
    // No need to call takeRtmpConnRef(), because this it rtmp_conn's callback.
    return client_session->rtmp_server.commandMessage (msg, msg_stream_id, amf_encoding);
}

void sendStateChanged (Sender::SendState   const send_state,
		       void              * const _client_session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    switch (send_state) {
	case Sender::ConnectionReady:
	    logD (framedrop, _func, "ConnectionReady");
#ifdef MOMENT_RTMP__FLOW_CONTROL
	    client_session->mutex.lock ();
	    client_session->overloaded = false;
	    client_session->mutex.unlock ();
#endif
	    break;
	case Sender::ConnectionOverloaded:
	    logD (framedrop, _func, "ConnectionOverloaded");
#ifdef MOMENT_RTMP__FLOW_CONTROL
	    client_session->mutex.lock ();
	    client_session->overloaded = true;
	    client_session->mutex.unlock ();
#endif
	    break;
	case Sender::QueueSoftLimit:
	    logD (framedrop, _func, "QueueSoftLimit");
	    // TODO Block input from the client.
	    break;
	case Sender::QueueHardLimit:
	    logD (framedrop, _func, "QueueHardLimit");
	    destroyClientSession (client_session);
	    // FIXME Close client connection
	    break;
	default:
	    unreachable();
    }
}

void closed (Exception * const exc,
	     void      * const _client_session)
{
    logD (mod_rtmp, _func, "client_session 0x", fmt_hex, (UintPtr) _client_session);

    if (exc)
	logE (mod_rtmp, _func, exc->toString());

    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);
    destroyClientSession (client_session);
}

RtmpConnection::Frontend const rtmp_frontend = {
    NULL /* handshakeComplete */,
    commandMessage,
    audioMessage /* audioMessage */,
    videoMessage /* videoMessage */,
    sendStateChanged,
    closed
};

Result clientConnected (RtmpConnection * mt_nonnull const rtmp_conn,
			void * const /* cb_data */)
{
    logD (mod_rtmp, _func_);

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

void momentRtmpInit ()
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

	if (enable == MConfig::Config::Boolean_False) {
	    logI_ (_func, "Unrestricted RTMP access module is not enabled. "
		   "Set \"", opt_name, "\" option to \"y\" to enable.");
	    return;
	}
    }

    {
	ConstMemory const opt_name = "mod_rtmp/send_delay";
	Uint64 send_delay_val = 50;
	MConfig::GetResult const res = config->getUint64_default (
		opt_name, &send_delay_val, send_delay_val);
	if (!res) {
	    logE_ (_func, "bad value for ", opt_name);
	} else {
	    logI_ (_func, "RTMP send delay: ", send_delay_val, " milliseconds");
	    rtmp_service.setSendDelay (send_delay_val);
	}
    }

    Time rtmpt_session_timeout = 30;
    {
	ConstMemory const opt_name = "mod_rtmp/rtmpt_session_timeout";
	MConfig::GetResult const res = config->getUint64_default (
		opt_name, &rtmpt_session_timeout, rtmpt_session_timeout);
	if (!res)
	    logE_ (_func, "bad value for ", opt_name);
    }
    logI_ (_func, "rtmpt_session_timeout: ", rtmpt_session_timeout);

    bool rtmpt_no_keepalive_conns = false;
    {
	ConstMemory const opt_name = "mod_rtmp/rtmpt_no_keepalive_conns";
	MConfig::Config::BooleanValue const enable = config->getBoolean (opt_name);
	if (enable == MConfig::Config::Boolean_Invalid) {
	    logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
	    return;
	}

	if (enable == MConfig::Config::Boolean_True)
	    rtmpt_no_keepalive_conns = true;
    }
    logD_ (_func, "rtmpt_no_keepalive_conns: ", rtmpt_no_keepalive_conns);

    {
	rtmp_service.setFrontend (Cb<RtmpVideoService::Frontend> (
		&rtmp_video_service_frontend, NULL, NULL));

	rtmp_service.setServerContext (server_app->getServerContext());
	rtmp_service.setPagePool (moment->getPagePool());

	if (!rtmp_service.init()) {
	    logE_ (_func, "rtmp_service.init() failed: ", exc->toString());
	    return;
	}

	do {
	    ConstMemory const opt_name = "mod_rtmp/rtmp_bind";
	    ConstMemory rtmp_bind = config->getString_default (opt_name, ":1935");

	    logD_ (_func, opt_name, ": ", rtmp_bind);
	    if (!rtmp_bind.isNull ()) {
		IpAddress addr;
		if (!setIpAddress_default (rtmp_bind,
					   ConstMemory() /* default_host */,
					   1935          /* default_port */,
					   true          /* allow_any_host */,
					   &addr))
		{
		    logE_ (_func, "setIpAddress_default() failed (rtmp)");
		    return;
		}

		if (!rtmp_service.bind (addr)) {
		    logE_ (_func, "rtmp_service.bind() failed: ", exc->toString());
		    break;
		}

		if (!rtmp_service.start ()) {
		    logE_ (_func, "rtmp_service.start() failed: ", exc->toString());
		    return;
		}
	    } else {
		logI_ (_func, "RTMP service is not bound to any port "
		       "and won't accept any connections. "
		       "Set \"", opt_name, "\" option to bind the service.");
	    }
	} while (0);
    }

    {
	rtmpt_service.setFrontend (Cb<RtmpVideoService::Frontend> (
		&rtmp_video_service_frontend, NULL, NULL));

	rtmpt_service.setTimers (server_app->getTimers());
	// TODO setServerContext()
	rtmpt_service.setPollGroup (server_app->getMainPollGroup());
	rtmpt_service.setPagePool (moment->getPagePool());

	if (!rtmpt_service.init (rtmpt_session_timeout, rtmpt_no_keepalive_conns)) {
	    logE_ (_func, "rtmpt_service.init() failed: ", exc->toString());
	    return;
	}

	do {
	    ConstMemory const opt_name = "mod_rtmp/rtmpt_bind";
	    ConstMemory const rtmpt_bind = config->getString_default (opt_name, ":8081");
	    logD_ (_func, opt_name, ": ", rtmpt_bind);
	    if (!rtmpt_bind.isNull ()) {
		IpAddress addr;
		if (!setIpAddress_default (rtmpt_bind,
					   ConstMemory() /* default_host */,
					   8081          /* default_port */,
					   true          /* allow_any_host */,
					   &addr))
		{
		    logE_ (_func, "setIpAddress_default() failed (rtmpt)");
		    return;
		}

		if (!rtmpt_service.bind (addr)) {
		    logE_ (_func, "rtmpt_service.bind() failed: ", exc->toString());
		    break;
		}

		if (!rtmpt_service.start ()) {
		    logE_ (_func, "rtmpt_service.start() failed: ", exc->toString());
		    return;
		}
	    } else {
		logI_ (_func, "RTMPT service is not bound to any port "
		       "and won't accept any connections. "
		       "Set \"", opt_name, "\" option to \"y\" to bind the service.");
	    }
	} while (0);
    }

    {
	ConstMemory const opt_name = "mod_rtmp/audio_waits_video";
	MConfig::Config::BooleanValue const opt_val = config->getBoolean (opt_name);
	if (opt_val == MConfig::Config::Boolean_Invalid) {
	    logE_ (_func, "Invalid value for config option ", opt_name);
	} else
	if (opt_val != MConfig::Config::Boolean_True) {
	    audio_waits_video = false;
	} else {
	    audio_waits_video = true;
	}
    }
}

void momentRtmpUnload ()
{
}

}

}


namespace M {

void libMary_moduleInit ()
{
    logD_ ("RTMP MODULE INIT");

    Moment::momentRtmpInit ();
}

void libMary_moduleUnload()
{
    logD_ ("RTMP MODULE UNLOAD");

    Moment::momentRtmpUnload ();
}

}

