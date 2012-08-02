/*  Moment Video Server - High performance media server
    Copyright (C) 2012 Dmitry Shatrov
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


#include <moment/rtmp_push_protocol.h>


using namespace M;

namespace Moment {

mt_mutex (mutex) void
RtmpPushConnection::destroySession (Session * const mt_nonnull session)
{
    session->rtmp_conn.close_noBackendCb ();

    if (session->pollable_key) {
        thread_ctx->getPollGroup()->removePollable (session->pollable_key);
        session->pollable_key = NULL;
    }
}

mt_mutex (mutex) void
RtmpPushConnection::startNewSession (Session * const old_session)
{
    logD_ (_func_);

    if (old_session) {
        if (old_session != cur_session) {
            logD_ (_func, "session mismatch: 0x", fmt_hex, (UintPtr) old_session, ", 0x", (UintPtr) cur_session.ptr());
            return;
        }

        destroySession (old_session);
    }

    logD_ (_func, "calling deleteReconnectTimer()");
    deleteReconnectTimer ();

    Ref<Session> const session = grab (new Session);
    cur_session = session;

    session->rtmp_push_conn = this;

    session->conn_sender.setConnection (&session->tcp_conn);
    session->conn_sender.setQueue (thread_ctx->getDeferredConnectionSenderQueue());

    session->conn_receiver.setConnection (&session->tcp_conn);
    session->conn_receiver.setFrontend (session->rtmp_conn.getReceiverFrontend());

    session->rtmp_conn.init (timers,
                             page_pool,
                             // It is expected that there will be few push RTMP conns.
                             // Using non-zero send delay gives negligible performance
                             // increase in this case.
                             0 /* send_delay_millisec */);

    // 'session' is surely referenced when a callback is called, because it serves
    // as a coderef container for 'rtmp_conn'. Same for 'tcp_conn'.
    session->rtmp_conn.setBackend (Cb<RtmpConnection::Backend> (&rtmp_conn_backend,
                                                                session /* cb_data */,
                                                                this /* coderef_container */));
    session->rtmp_conn.setFrontend (Cb<RtmpConnection::Frontend> (&rtmp_conn_frontend,
                                                                  session /* cb_data */,
                                                                  this /* coderef_container */));
    session->rtmp_conn.setSender (&session->conn_sender);

    session->tcp_conn.setFrontend (Cb<TcpConnection::Frontend> (&tcp_conn_frontend,
                                                                session /* cb_data */,
                                                                this /* coderef_container */));

    if (!session->tcp_conn.connect (server_addr)) {
        logE_ (_func, "Could not connect to server: ", exc->toString());

        destroySession (session);
        cur_session = NULL;

        setReconnectTimer ();
        return;
    }

    session->pollable_key = thread_ctx->getPollGroup()->addPollable (session->tcp_conn.getPollable(),
                                                                     NULL /* ret_reg */);
}

mt_mutex (mutex) void
RtmpPushConnection::setReconnectTimer ()
{
    logD_ (_func_);

    logD_ (_func, "calling deleteReconnectTimer()");
    deleteReconnectTimer ();

    reconnect_timer = timers->addTimer (CbDesc<Timers::TimerCallback> (reconnectTimerTick,
                                                                       this /* cb_data */,
                                                                       this /* coderef_container */),
                                        // TODO Config parameter for the timeout.
                                        1     /* time_seconds */,
                                        false /* periodical */);
}

mt_mutex (mutex) void
RtmpPushConnection::deleteReconnectTimer ()
{
    logD_ (_func_);

    if (reconnect_timer) {
        timers->deleteTimer (reconnect_timer);
        reconnect_timer = NULL;
    }
}

void
RtmpPushConnection::reconnectTimerTick (void * const _self)
{
    logD_ (_func_);

    RtmpPushConnection * const self = static_cast <RtmpPushConnection*> (_self);

    self->mutex.lock ();
    logD_ (_func, "calling deleteReconnectTimer()");
    self->deleteReconnectTimer ();

    if (self->cur_session) {
        self->mutex.unlock ();
        return;
    }

    self->startNewSession (NULL /* old_session */);
    self->mutex.unlock ();
}

void
RtmpPushConnection::scheduleReconnect (Session * const old_session)
{
    logD_ (_func, "session 0x", fmt_hex, (UintPtr) old_session);

    mutex.lock ();
    if (old_session != cur_session) {
        logD_ (_func, "session mismatch: 0x", fmt_hex, (UintPtr) old_session, ", 0x", (UintPtr) cur_session.ptr());
        mutex.unlock ();
        return;
    }

    destroySession (old_session);
    cur_session = NULL;

    setReconnectTimer ();

    mutex.unlock ();
}

TcpConnection::Frontend const RtmpPushConnection::tcp_conn_frontend = {
    connected
};

void
RtmpPushConnection::connected (Exception * const exc_,
                               void      * const _session)
{
    logD_ (_func_);

    Session * const session = static_cast <Session*> (_session);
    RtmpPushConnection * const self = session->rtmp_push_conn;

    if (exc_) {
        logE_ (_func, "Could not connect to server: ", exc_->toString());
        self->scheduleReconnect (session);
        return;
    }

    session->rtmp_conn.startClient ();
}

RtmpConnection::Backend const RtmpPushConnection::rtmp_conn_backend = {
    closeRtmpConn
};

void
RtmpPushConnection::closeRtmpConn (void * const _session)
{
    logD_ (_func_);

    Session * const session = static_cast <Session*> (_session);
    RtmpPushConnection * const self = session->rtmp_push_conn;

    self->scheduleReconnect (session);
}

RtmpConnection::Frontend const RtmpPushConnection::rtmp_conn_frontend = {
    handshakeComplete,
    commandMessage,
    NULL /* audioMessage */,
    NULL /* videoMessage */,
    NULL /* sendStateChanged */,
    closed
};

Result
RtmpPushConnection::handshakeComplete (void * const _session)
{
    Session * const session = static_cast <Session*> (_session);
    RtmpPushConnection * const self = session->rtmp_push_conn;

    session->conn_state = ConnectionState_ConnectSent;
    session->rtmp_conn.sendConnect (self->app_name->mem());

    return Result::Success;
}

Result
RtmpPushConnection::commandMessage (VideoStream::Message * const mt_nonnull msg,
                                    Uint32                 const /* msg_stream_id */,
                                    AmfEncoding            const /* amf_encoding */,
                                    void                 * const _session)
{
    Session * const session = static_cast <Session*> (_session);
    RtmpPushConnection * const self = session->rtmp_push_conn;

    if (msg->msg_len == 0)
        return Result::Success;

    PagePool::PageListArray pl_array (msg->page_list.first, msg->msg_len);
    AmfDecoder decoder (AmfEncoding::AMF0, &pl_array, msg->msg_len);

    Byte method_name [256];
    Size method_name_len;
    if (!decoder.decodeString (Memory::forObject (method_name),
                               &method_name_len,
                               NULL /* ret_full_len */))
    {
        logE_ (_func, "Could not decode method name");
        return Result::Failure;
    }

    ConstMemory method (method_name, method_name_len);
    if (!compare (method, "_result")) {
        switch (session->conn_state) {
            case ConnectionState_ConnectSent: {
                session->rtmp_conn.sendCreateStream ();
                session->conn_state = ConnectionState_CreateStreamSent;
            } break;
            case ConnectionState_CreateStreamSent: {
                double stream_id;
                if (!decoder.decodeNumber (&stream_id)) {
                    logE_ (_func, "Could not decode stream_id");
                    return Result::Failure;
                }

                session->rtmp_conn.sendPublish (self->stream_name->mem());

                session->conn_state = ConnectionState_Streaming;
                // TODO Send saved frames.
                session->publishing.set (1);
            } break;
            case ConnectionState_PublishSent: {
              // Unused
            } break;
            default:
              // Ignoring
                ;
        }
    } else
    if (!compare (method, "_error")) {
        switch (session->conn_state) {
            case ConnectionState_ConnectSent:
            case ConnectionState_CreateStreamSent:
            case ConnectionState_PublishSent: {
                logE_ (_func, "_error received, returning Failure");
                return Result::Failure;
            } break;
            default:
              // Ignoring
                ;
        }
    } else {
        logW_ (_func, "unknown method: ", method);
    }

    return Result::Success;
}

void
RtmpPushConnection::closed (Exception * const exc_,
                            void      * const /* _session */)
{
    if (exc_) {
        logE_ (_func, exc_->toString());
    } else
        logE_ (_func_);
}

VideoStream::EventHandler const RtmpPushConnection::video_event_handler = {
    audioMessage,
    videoMessage,
    NULL /* rtmpCommandMessage */,
    NULL /* closed */,
    NULL /* numWatchersChanged */
};

void
RtmpPushConnection::audioMessage (VideoStream::AudioMessage * const mt_nonnull msg,
                                  void                      * const _self)
{
    RtmpPushConnection * const self = static_cast <RtmpPushConnection*> (_self);

    self->mutex.lock ();
    Ref<Session> const session = self->cur_session;
    self->mutex.unlock ();

    if (!session)
        return;

    if (session->publishing.get() == 1)
        session->rtmp_conn.sendAudioMessage (msg);
}

void
RtmpPushConnection::videoMessage (VideoStream::VideoMessage * const mt_nonnull msg,
                                  void                      * const _self)
{
    RtmpPushConnection * const self = static_cast <RtmpPushConnection*> (_self);

    self->mutex.lock ();
    Ref<Session> const session = self->cur_session;
    self->mutex.unlock ();

    if (!session)
        return;

    if (session->publishing.get() == 1) {
      // TODO Wait for keyframe. Move keyframe awaiting logics
      //      from mod_rtmp to RtmpConnection.
      //      The trickier part is making this work while
      //      sending saved frames in advance.
        session->rtmp_conn.sendVideoMessage (msg);
    }
}

mt_const void
RtmpPushConnection::init (ServerThreadContext * const mt_nonnull _thread_ctx,
                          PagePool            * const mt_nonnull _page_pool,
                          VideoStream         * const _video_stream,
                          IpAddress             const _server_addr,
                          ConstMemory           const _username,
                          ConstMemory           const _password,
                          ConstMemory           const _app_name,
                          ConstMemory           const _stream_name)
{
    thread_ctx = _thread_ctx;
    timers = thread_ctx->getTimers();
    page_pool = _page_pool;

    video_stream = _video_stream;

    server_addr = _server_addr;
    username = grab (new String (_username));
    password = grab (new String (_password));
    app_name = grab (new String (_app_name));
    stream_name = grab (new String (_stream_name));

    mutex.lock ();
    startNewSession (NULL /* old_session */);
    mutex.unlock ();

    video_stream->getEventInformer()->subscribe (
            CbDesc<VideoStream::EventHandler> (&video_event_handler,
                                               this /* cb_data */,
                                               this /* coderef_container */));
}

RtmpPushConnection::RtmpPushConnection ()
    : reconnect_timer (NULL)
{
}

RtmpPushConnection::~RtmpPushConnection ()
{
    mutex.lock ();

    logD_ (_func, "calling deleteReconnectTimer()");
    deleteReconnectTimer ();

    if (cur_session) {
        destroySession (cur_session);
        cur_session = NULL;
    }

    mutex.unlock ();
}

#if 0
// Unused
static void skipUriWhitespace (ConstMemory     const uri,
                               unsigned long * const mt_nonnull ret_pos)
{
    unsigned long pos = *ret_pos;

    while (pos < uri.len()) {
        if (uri.mem() [pos] != ' ' && uri.mem() [pos] != '\t')
            break;
        ++pos;
    }

    *ret_pos = pos;
}
#endif

mt_throws Ref<PushConnection>
RtmpPushProtocol::connect (VideoStream * const video_stream,
                           ConstMemory   const uri,
                           ConstMemory   const username,
                           ConstMemory   const password)
{
    logD_ (_func, "uri: ", uri);

    IpAddress server_addr;
    // TODO Parse application name, channel name.
    Ref<String> const app_name = grab (new String ("app"));
    Ref<String> const stream_name = grab (new String ("stream"));
    {
      // URI forms:   rtmp://user:password@host:port/foo/bar
      //              rtmp://host:port/foo/bar
      //
      // Note that we do not extract user:password from the URI but rather
      // accept them as separate function parameters. This is inconsistent.
      // It might be convenient to parse user:password and use them
      // instead of explicit parameters when the latter are null.

        unsigned long pos = 0;
        while (pos < uri.len()) {
            if (uri.mem() [pos] == '/')
                break;
            ++pos;
        }
        ++pos;

        while (pos < uri.len()) {
            if (uri.mem() [pos] == '/')
                break;
            ++pos;
        }
        ++pos;

        // user:password@host:port
        unsigned long const user_addr_begin = pos;

        while (pos < uri.len()) {
            if (uri.mem() [pos] == '/')
                break;
            ++pos;
        }

        ConstMemory const user_addr = uri.region (user_addr_begin, pos - user_addr_begin);
        logD_ (_func, "user_addr_begin: ", user_addr_begin, ", pos: ", pos, ", user_addr: ", user_addr);
        unsigned long at_pos = 0;
        bool got_at = false;
        while (at_pos < user_addr.len()) {
            if (user_addr.mem() [at_pos] == '@') {
                got_at = true;
                break;
            }
            ++at_pos;
        }

        ConstMemory addr_mem = user_addr;
        if (got_at)
            addr_mem = user_addr.region (at_pos + 1, user_addr.len() - (at_pos + 1));

        logD_ (_func, "addr_mem: ", addr_mem);
        if (!setIpAddress (addr_mem, &server_addr)) {
            logE_ (_func, "Could not extract address from URI: ", uri);
            goto _failure;
        }
    }

  {
    Ref<RtmpPushConnection> const rtmp_push_conn = grab (new RtmpPushConnection);
    rtmp_push_conn->init (moment->getServerApp()->getServerContext()->selectThreadContext(),
                          moment->getPagePool(),
                          video_stream,
                          server_addr,
                          username,
                          password,
                          app_name->mem(),
                          stream_name->mem());

    return rtmp_push_conn;
  }

_failure:
    exc_throw <InternalException> (InternalException::BadInput);
    return NULL;
}

mt_const void
RtmpPushProtocol::init (MomentServer * const mt_nonnull moment)
{
    this->moment = moment;
}

}

