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
#include <moment/rtmp_push_protocol.h>


// Needed for "start_paused" as well.
// TODO This belongs to class RtmpConnection.
//#define MOMENT_RTMP__WAIT_FOR_KEYFRAME

//#define MOMENT_RTMP__AUDIO_WAITS_VIDEO

// Flow control is disabled until done right.
//#define MOMENT_RTMP__FLOW_CONTROL


namespace Moment {

namespace {

static LogGroup libMary_logGroup_mod_rtmp ("mod_rtmp", LogLevel::I);
static LogGroup libMary_logGroup_session ("mod_rtmp.session", LogLevel::I);
static LogGroup libMary_logGroup_framedrop ("mod_rtmp.framedrop", LogLevel::I);

class MomentRtmpModule : public Object
{
public:
    RtmpService  rtmp_service;
    RtmptService rtmpt_service;

    MomentRtmpModule ()
          // TODO Is it possible to pass NULL as coderef_container?
          //      That would make sense since MomentRtmpModule is effectively
          //      a global object.
        : rtmp_service  (this /* coderef_container */),
          rtmpt_service (this /* coderef_container */)
    {
    }
};

mt_const bool audio_waits_video = false;
mt_const bool default_start_paused = false;

mt_const bool record_all = false;
mt_const ConstMemory record_path = "/opt/moment/records";
mt_const Uint64 recording_limit = 1 << 24 /* 16 Mb */;

#ifdef MOMENT_RTMP__WAIT_FOR_KEYFRAME
mt_const Count no_keyframe_limit = 250; // 25 fps * 10 seconds
#endif

class WatchingParams
{
public:
    bool start_paused;

    void reset ()
    {
	start_paused = default_start_paused;
    }

    WatchingParams ()
    {
	reset ();
    }
};

class ClientSession : public Object
{
public:
    mt_mutex (mutex) bool valid;

    IpAddress client_addr;

    mt_const RtmpConnection *rtmp_conn;
    // Remember that RtmpConnection must be available when we're calling
    // RtmpServer's methods. We must take special care to ensure that this
    // holds. See takeRtmpConnRef().
    RtmpServer rtmp_server;

    ServerThreadContext *recorder_thread_ctx;
    AvRecorder recorder;
    FlvMuxer flv_muxer;

    mt_mutex (mutex) Ref<MomentServer::ClientSession> srv_session;

    mt_mutex (mutex) Ref<VideoStream> video_stream;
    // TODO Deprecated field
    mt_mutex (mutex) MomentServer::VideoStreamKey video_stream_key;

    mt_mutex (mutex) Ref<VideoStream> watching_video_stream;

    mt_mutex (mutex) WatchingParams watching_params;

#ifdef MOMENT_RTMP__FLOW_CONTROL
    mt_mutex (mutex) bool overloaded;
#endif

#ifdef MOMENT_RTMP__WAIT_FOR_KEYFRAME
    // Used from streamVideoMessage() only.
    mt_mutex (mutex) Count no_keyframe_counter;
    mt_mutex (mutex) bool keyframe_sent;
    mt_mutex (mutex) bool first_keyframe_sent;
#endif

    mt_mutex (mutex) bool resumed;

    // Synchronized by rtmp_server.
    bool streaming;
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
	  recorder_thread_ctx (NULL),
	  recorder (this),
#ifdef MOMENT_RTMP__FLOW_CONTROL
	  overloaded (false),
#endif
#ifdef MOMENT_RTMP__WAIT_FOR_KEYFRAME
	  no_keyframe_counter (0),
	  keyframe_sent (false),
	  first_keyframe_sent (false),
#endif
	  resumed (false),
	  streaming (false),
	  watching (false)
    {
	logD (session, _func, "0x", fmt_hex, (UintPtr) this);
    }

    ~ClientSession ()
    {
	logD (session, _func, "0x", fmt_hex, (UintPtr) this);

	MomentServer * const moment = MomentServer::getInstance();

	if (recorder_thread_ctx) {
	    moment->getRecorderThreadPool()->releaseThreadContext (recorder_thread_ctx);
	    recorder_thread_ctx = NULL;
	}
    }
};

void destroyClientSession (ClientSession * const client_session)
{
    client_session->recorder.stop();

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

    if (video_stream_key)
	moment->removeVideoStream (video_stream_key);

    // Closing video stream *after* firing clientDisconnected() to avoid
    // premature closing of client connections in streamClosed().
    if (video_stream)
	video_stream->close ();

    client_session->unref ();
}

void streamAudioMessage (VideoStream::AudioMessage * const mt_nonnull msg,
			 void                      * const _session)
{
//    logD_ (_func_);

    ClientSession * const client_session = static_cast <ClientSession*> (_session);

    Ref<Object> rtmp_conn_ref;
    // TODO client_session->mutex is locked/unlocked here, and then we lock it again
    //      in the likely path. That's not effective.
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

    client_session->rtmp_conn->sendAudioMessage (msg);
}

void streamVideoMessage (VideoStream::VideoMessage * const mt_nonnull msg,
			 void                      * const _session)
{
//    logD_ (_func, "ts 0x", fmt_hex, msg->timestamp, " ", msg->frame_type, (msg->is_saved_frame ? " SAVED" : ""));

    ClientSession * const client_session = static_cast <ClientSession*> (_session);

    Ref<Object> rtmp_conn_ref;
    // TODO client_session->mutex is locked/unlocked here, and then we lock it again
    //      in the likely path. That's not effective.
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
    if (!msg->is_saved_frame
        && (msg->frame_type == VideoStream::VideoFrameType::KeyFrame ||
	    msg->frame_type == VideoStream::VideoFrameType::GeneratedKeyFrame))
    {
//        logD_ (_func, "KEYFRAME");
	got_keyframe = true;
    } else
    if (msg->frame_type == VideoStream::VideoFrameType::AvcSequenceHeader ||
        msg->frame_type == VideoStream::VideoFrameType::AvcEndOfSequence)
    {
//        logD_ (_func, "KEYFRAME NOT SENT");
        client_session->keyframe_sent = false;
    } else
    if (!client_session->keyframe_sent
	&& (   msg->frame_type == VideoStream::VideoFrameType::InterFrame
	    || msg->frame_type == VideoStream::VideoFrameType::DisposableInterFrame))
    {
	++client_session->no_keyframe_counter;
	if (client_session->no_keyframe_counter >= no_keyframe_limit) {
            logD_ (_func, "no_keyframe_limit hit: ", client_session->no_keyframe_counter);
	    got_keyframe = true;
	} else {
	  // Waiting for a keyframe, dropping current video frame.
	    client_session->mutex.unlock ();
//            logD_ (_func, "DROPPING FRAME");
	    return;
	}
    }

    if (got_keyframe)
	client_session->no_keyframe_counter = 0;

    if (client_session->watching_params.start_paused &&
	!client_session->resumed &&
        msg->frame_type.isVideoData())
    {
	bool match = client_session->first_keyframe_sent;
	if (!match) {
	    if (client_session->watching_video_stream) {
		// TODO No lock inversion?
		client_session->watching_video_stream->lock ();
		match = client_session->watching_video_stream->getFrameSaver()->getSavedKeyframe (NULL);
		client_session->watching_video_stream->unlock ();
	    }
	}

	if (match) {
	    client_session->mutex.unlock ();
//            logD_ (_func, "START PAUSED, DROPPING");
	    return;
	}
    }

    if (got_keyframe) {
	client_session->keyframe_sent = true;
	client_session->first_keyframe_sent = true;
    }
#endif

    client_session->mutex.unlock ();

//    logD_ (_func, "sending ", toString (msg->codec_id), ", ", toString (msgo->frame_type));

    client_session->rtmp_conn->sendVideoMessage (msg);
}

void streamClosed (void * const _session)
{
    logD (session, _func_);
    ClientSession * const client_session = static_cast <ClientSession*> (_session);
// Unnecessary    destroyClientSession (client_session);
    client_session->rtmp_conn->closeAfterFlush ();
}

VideoStream::EventHandler const video_event_handler = {
    streamAudioMessage,
    streamVideoMessage,
    NULL /* rtmpCommandMessage */,
    streamClosed,
    NULL /* numWatchersChanged */
};

Result connect (ConstMemory const &app_name,
		void * const _client_session)
{
    logD (session, _func, "app_name: ", app_name);

    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    // TODO class MomentRtmpModule
    MomentServer * const moment = MomentServer::getInstance();

    Ref<MomentServer::ClientSession> const srv_session =
	    moment->rtmpClientConnected (app_name, client_session->rtmp_conn, client_session->client_addr);
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

typedef void (*ParameterCallback) (ConstMemory  name,
				   ConstMemory  value,
				   void        *cb_data);

// Very similar to M::HttpRequest::parseParameters().
static void parseParameters (ConstMemory         const mem,
			     ParameterCallback   const param_cb,
			     void              * const param_cb_data)
{
    Byte const *uri_end = mem.mem() + mem.len();
    Byte const *param_pos = mem.mem();

    while (param_pos < uri_end) {
	ConstMemory name;
	ConstMemory value;
	Byte const *value_start = (Byte const *) memchr (param_pos, '=', uri_end - param_pos);
	if (value_start) {
	    ++value_start; // Skipping '='
	    if (value_start > uri_end)
		value_start = uri_end;

	    name = ConstMemory (param_pos, value_start - 1 /*'='*/ - param_pos);

	    Byte const *value_end = (Byte const *) memchr (value_start, '&', uri_end - value_start);
	    if (value_end) {
		if (value_end > uri_end)
		    value_end = uri_end;

		value = ConstMemory (value_start, value_end - value_start);
		param_pos = value_end + 1; // Skipping '&'
	    } else {
		value = ConstMemory (value_start, uri_end - value_start);
		param_pos = uri_end;
	    }
	} else {
	    name = ConstMemory (param_pos, uri_end - param_pos);
	    param_pos = uri_end;
	}

	logD_ (_func, "parameter: ", name, " = ", value);

	param_cb (name, value, param_cb_data);
    }
}


Result startStreaming (ConstMemory     const &_stream_name,
		       RecordingMode   const rec_mode,
		       void          * const _client_session)
{
    logD (session, _func, "stream_name: ", _stream_name);

    // TODO class MomentRtmpModule
    MomentServer * const moment = MomentServer::getInstance();

    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    if (client_session->streaming) {
	logE (mod_rtmp, _func, "already streaming another stream");
	return Result::Success;
    }
    client_session->streaming = true;

    ConstMemory stream_name = _stream_name;
    {
      // This will be unnecessary after parameter parsing is implemented in HttpServer.
      // 12.01.17 ^^ ? How's this related?
	Byte const * const name_sep = (Byte const *) memchr (stream_name.mem(), '?', stream_name.len());
	if (name_sep)
	    stream_name = stream_name.region (0, name_sep - stream_name.mem());
    }

    // 'srv_session' is created in connect(), which is synchronized with
    // startStreaming(). No locking needed.
    Ref<VideoStream> const video_stream =
	    moment->startStreaming (client_session->srv_session, stream_name, rec_mode);
    if (!video_stream)
	return Result::Failure;

    if (record_all) {
	logD_ (_func, "rec_mode: ", (Uint32) rec_mode);
	if (rec_mode == RecordingMode::Replace ||
	    rec_mode == RecordingMode::Append)
	{
	    logD_ (_func, "recording");
	    // TODO Support "append" mode.
	    client_session->recorder.setVideoStream (video_stream);
	    client_session->recorder.start (
		    makeString (record_path, stream_name, ".flv")->mem());
	}
    }

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

void startWatching_paramCallback (ConstMemory   const name,
				  ConstMemory   const /* value */,
				  void        * const _watching_params)
{
    WatchingParams * const watching_params = static_cast <WatchingParams*> (_watching_params);

    if (equal (name, "paused"))
	watching_params->start_paused = true;
}

static mt_mutex (client_session) Result
savedAudioFrame (VideoStream::AudioMessage * const mt_nonnull audio_msg,
                 void                      * const _client_session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);
    client_session->rtmp_conn->sendAudioMessage (audio_msg);
    return Result::Success;
}

static mt_mutex (client_session) Result
savedVideoFrame (VideoStream::VideoMessage * const mt_nonnull video_msg,
                 void                      * const _client_session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);
    client_session->rtmp_conn->sendVideoMessage (video_msg);
    return Result::Success;
}

static VideoStream::FrameSaver::FrameHandler const saved_frame_handler = {
    savedAudioFrame,
    savedVideoFrame
};

Result startWatching (ConstMemory const &_stream_name,
		      void * const _client_session)
{
    // TODO class MomentRtmpModule
    MomentServer * const moment = MomentServer::getInstance();

    logD (mod_rtmp, _func, "client_session 0x", fmt_hex, (UintPtr) _client_session);
    logD (mod_rtmp, _func, "stream_name: ", _stream_name);

    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    if (client_session->watching) {
	logE (mod_rtmp, _func, "already watching another stream");
	return Result::Success;
    }
    client_session->watching = true;

    ConstMemory stream_name = _stream_name;

    client_session->mutex.lock ();
    client_session->watching_params.reset ();
    client_session->resumed = false;
    {
	Byte const * const name_sep = (Byte const *) memchr (stream_name.mem(), '?', stream_name.len());
	if (name_sep) {
	    parseParameters (stream_name.region (name_sep + 1 - stream_name.mem()), startWatching_paramCallback, &client_session->watching_params);
	    stream_name = stream_name.region (0, name_sep - stream_name.mem());
	}
    }
    client_session->mutex.unlock ();

    Ref<VideoStream> video_stream;
    for (unsigned long watchdog_cnt = 0; watchdog_cnt < 100; ++watchdog_cnt) {
        video_stream = moment->startWatching (client_session->srv_session, stream_name);
        if (!video_stream) {
            logD (mod_rtmp, _func, "video stream not found: ", stream_name);
            return Result::Failure;
        }

        // TODO Repetitive locking of 'client_session' - bad.
        client_session->mutex.lock ();
        // TODO Set watching_video_stream to NULL when it's not needed anymore.
        client_session->watching_video_stream = video_stream;

        video_stream->lock ();
        if (video_stream->isClosed_unlocked()) {
            video_stream->unlock ();

            client_session->watching_video_stream = NULL;
            client_session->mutex.unlock ();
            continue;
        }

        video_stream->getFrameSaver()->reportSavedFrames (&saved_frame_handler, client_session);
        client_session->mutex.unlock ();

        video_stream->getEventInformer()->subscribe_unlocked (&video_event_handler,
                                                              client_session,
                                                              NULL /* ref_data */,
                                                              client_session);
        mt_async mt_unlocks_locks (video_stream->mutex) video_stream->plusOneWatcher_unlocked (client_session /* guard_obj */);
        video_stream->unlock ();
        break;
    }
    if (!video_stream) {
        logH_ (_func, "startWatching() watchdog counter hit");
        return Result::Failure;
    }

    return Result::Success;
}

RtmpServer::CommandResult server_commandMessage (RtmpConnection       * const mt_nonnull conn,
						 Uint32                 const msg_stream_id,
						 ConstMemory const    &method_name,
						 VideoStream::Message * const mt_nonnull msg,
						 AmfDecoder           * const mt_nonnull amf_decoder,
						 void                 * const _client_session)
{
    logD (session, _func, "method_name: ", method_name);

    MomentServer * const moment = MomentServer::getInstance();
    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    if (equal (method_name, "resume")) {
	// TODO Unused, we never get here.

	client_session->mutex.lock ();
	client_session->resumed = true;
	client_session->mutex.unlock ();

	conn->doBasicMessage (msg_stream_id, amf_decoder);
    } else {
	client_session->mutex.lock ();
	Ref<MomentServer::ClientSession> const srv_session = client_session->srv_session;
	client_session->mutex.unlock ();

	if (!srv_session) {
	    logW_ (_func, "No server session, command message dropped");
	    return RtmpServer::CommandResult::UnknownCommand;
	}

	moment->rtmpCommandMessage (srv_session, conn, /* TODO Unnecessary? msg_stream_id, */ msg, method_name, amf_decoder);
    }

    return RtmpServer::CommandResult::Success;
}

static Result pauseCmd (void * const /* _client_session */)
{
  // No-op
    logD_ (_func_);
    return Result::Success;
}

static Result resumeCmd (void * const _client_session)
{
    logD_ (_func_);

    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    client_session->mutex.lock ();
    client_session->resumed = true;
    client_session->mutex.unlock ();

    return Result::Success;
}

static RtmpServer::Frontend const rtmp_server_frontend = {
    connect,
    startStreaming /* startStreaming */,
    startWatching,
    server_commandMessage,
    pauseCmd,
    resumeCmd
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
    // No need to call takeRtmpConnRef(), because this is rtmp_conn's callback.
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
	logD (mod_rtmp, _func, exc->toString());

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

Result clientConnected (RtmpConnection  * const mt_nonnull rtmp_conn,
			IpAddress const &client_addr,
			void            * const /* cb_data */)
{
    logD (mod_rtmp, _func_);
//    logD_ (_func, "--- client_addr: ", client_addr);

    Ref<ClientSession> const client_session = grab (new ClientSession);
    client_session->client_addr = client_addr;
    client_session->rtmp_conn = rtmp_conn;

    {
	MomentServer * const moment = MomentServer::getInstance();

	ServerThreadContext *thread_ctx =
		moment->getRecorderThreadPool()->grabThreadContext ("flash" /* TODO Configurable prefix */);
	if (thread_ctx) {
	    client_session->recorder_thread_ctx = thread_ctx;
	} else {
	    logE_ (_func, "Couldn't get recorder thread context: ", exc->toString());
	    thread_ctx = moment->getServerApp()->getMainThreadContext();
	}

	client_session->flv_muxer.setPagePool (moment->getPagePool());

	client_session->recorder.init (thread_ctx, moment->getStorage());
	client_session->recorder.setRecordingLimit (recording_limit);
	client_session->recorder.setMuxer (&client_session->flv_muxer);
	// TODO recorder frontend + error reporting
    }

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

static void serverDestroy (void * const _rtmp_module)
{
    MomentRtmpModule * const rtmp_module = static_cast <MomentRtmpModule*> (_rtmp_module);

    logH_ (_func_);
    rtmp_module->unref ();
}

static MomentServer::Events const server_events = {
    serverDestroy
};

void momentRtmpInit ()
{
    MomentServer * const moment = MomentServer::getInstance();
    ServerApp * const server_app = moment->getServerApp();
    MConfig::Config * const config = moment->getConfig();

    {
        Ref<RtmpPushProtocol> const rtmp_push_proto = grab (new RtmpPushProtocol);
        rtmp_push_proto->init (moment);
        moment->addPushProtocol ("rtmp", rtmp_push_proto);
    }

    MomentRtmpModule * const rtmp_module = new MomentRtmpModule;
    moment->getEventInformer()->subscribe (CbDesc<MomentServer::Events> (&server_events, rtmp_module, NULL));

    {
	ConstMemory const opt_name = "mod_rtmp/enable";
	MConfig::BooleanValue const enable = config->getBoolean (opt_name);
	if (enable == MConfig::Boolean_Invalid) {
	    logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
	    return;
	}

	if (enable == MConfig::Boolean_False) {
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
	    rtmp_module->rtmp_service.setSendDelay (send_delay_val);
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
	MConfig::BooleanValue const enable = config->getBoolean (opt_name);
	if (enable == MConfig::Boolean_Invalid) {
	    logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
	    return;
	}

	if (enable == MConfig::Boolean_True)
	    rtmpt_no_keepalive_conns = true;

	logI_ (_func, opt_name, ": ", rtmpt_no_keepalive_conns);
    }

    {
	ConstMemory const opt_name = "mod_rtmp/record_all";
	MConfig::BooleanValue const value = config->getBoolean (opt_name);
	if (value == MConfig::Boolean_Invalid) {
	    logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name),
		   ", assuming \"", record_all, "\"");
	} else {
	    if (value == MConfig::Boolean_True)
		record_all = true;
	    else
		record_all = false;

	    logI_ (_func, opt_name, ": ", record_all);
	}
    }

    record_path = config->getString_default ("mod_rtmp/record_path", record_path);

    {
	ConstMemory const opt_name = "mod_rtmp/record_limit";
	MConfig::GetResult const res = config->getUint64_default (
		opt_name, &recording_limit, recording_limit);
	if (!res)
	    logE_ (_func, "bad value for ", opt_name);

	logI_ (_func, opt_name, ": ", recording_limit);
    }

    {
	rtmp_module->rtmp_service.setFrontend (Cb<RtmpVideoService::Frontend> (
		&rtmp_video_service_frontend, NULL, NULL));

	rtmp_module->rtmp_service.setServerContext (server_app->getServerContext());
	rtmp_module->rtmp_service.setPagePool (moment->getPagePool());

	if (!rtmp_module->rtmp_service.init()) {
	    logE_ (_func, "rtmp_service.init() failed: ", exc->toString());
	    return;
	}

	do {
	    ConstMemory const opt_name = "mod_rtmp/rtmp_bind";
	    ConstMemory rtmp_bind = config->getString_default (opt_name, ":1935");

	    logI_ (_func, opt_name, ": ", rtmp_bind);
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

		if (!rtmp_module->rtmp_service.bind (addr)) {
		    logE_ (_func, "rtmp_service.bind() failed: ", exc->toString());
		    break;
		}

		if (!rtmp_module->rtmp_service.start ()) {
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
	rtmp_module->rtmpt_service.setFrontend (Cb<RtmpVideoService::Frontend> (
		&rtmp_video_service_frontend, NULL, NULL));

	if (!rtmp_module->rtmpt_service.init (server_app->getServerContext()->getTimers(),
                                 moment->getPagePool(),
                                 // TODO setServerContext()
                                 // TODO Pick a server thread context and pass it here.
                                 server_app->getServerContext()->getMainPollGroup(),
                                 server_app->getMainThreadContext()->getDeferredProcessor(),
                                 rtmpt_session_timeout,
                                 rtmpt_no_keepalive_conns))
        {
	    logE_ (_func, "rtmpt_service.init() failed: ", exc->toString());
	    return;
	}

	do {
	    ConstMemory const opt_name = "mod_rtmp/rtmpt_bind";
	    ConstMemory const rtmpt_bind = config->getString_default (opt_name, ":8081");
	    logI_ (_func, opt_name, ": ", rtmpt_bind);
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

		if (!rtmp_module->rtmpt_service.bind (addr)) {
		    logE_ (_func, "rtmpt_service.bind() failed: ", exc->toString());
		    break;
		}

		if (!rtmp_module->rtmpt_service.start ()) {
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
	ConstMemory const opt_name = "mod_rtmp/rtmpt_from_http";
	MConfig::BooleanValue const opt_val = config->getBoolean (opt_name);
	if (opt_val == MConfig::Boolean_Invalid)
	    logE_ (_func, "Invalid value for config option ", opt_name);
	else
	if (opt_val != MConfig::Boolean_False)
	    rtmp_module->rtmpt_service.getRtmptServer()->attachToHttpService (moment->getHttpService());
    }

    {
	ConstMemory const opt_name = "mod_rtmp/audio_waits_video";
	MConfig::BooleanValue const opt_val = config->getBoolean (opt_name);
	if (opt_val == MConfig::Boolean_Invalid)
	    logE_ (_func, "Invalid value for config option ", opt_name);
	else
	if (opt_val == MConfig::Boolean_True)
	    audio_waits_video = true;
	else
	    audio_waits_video = false;
    }

    {
	ConstMemory const opt_name = "mod_rtmp/start_paused";
	MConfig::BooleanValue const opt_val = config->getBoolean (opt_name);
	if (opt_val == MConfig::Boolean_Invalid)
	    logE_ (_func, "Invalid value for config option ", opt_name);
	else
	if (opt_val == MConfig::Boolean_True)
	    default_start_paused = true;
	else
	    default_start_paused = false;

	logD_ (_func, "default_start_paused: ", default_start_paused);
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
    logI_ (_func, "Initializing mod_rtmp");

    Moment::momentRtmpInit ();
}

void libMary_moduleUnload()
{
    logI_ (_func, "Unloading mod_rtmp");

    Moment::momentRtmpUnload ();
}

}

