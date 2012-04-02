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


#include <moment/rtmpt_server.h>


// TODO 1. keepalive timers.


// Hint: Don't put commas after *HEADERS macros when using them.

#define RTMPT_SERVER__HEADERS_DATE \
	Byte date_buf [timeToString_BufSize]; \
	Size const date_len = timeToString (Memory::forObject (date_buf), getUnixtime());

#define RTMPT_SERVER__COMMON_HEADERS(keepalive) \
	"Server: Moment/1.0\r\n" \
	"Date: ", ConstMemory (date_buf, date_len), "\r\n" \
	"Connection: ", (keepalive) ? "Keep-Alive" : "Close", "\r\n" \
	"Cache-Control: no-cache\r\n"

#define RTMPT_SERVER__OK_HEADERS(keepalive) \
	"HTTP/1.", ((keepalive) ? "1" : "1"), " 200 OK\r\n" \
	RTMPT_SERVER__COMMON_HEADERS(keepalive)

#define RTMPT_SERVER__FCS_OK_HEADERS(keepalive) \
	RTMPT_SERVER__OK_HEADERS(keepalive) \
	"Content-Type: application/x-fcs\r\n" \

#define RTMPT_SERVER__404_HEADERS(keepalive) \
	"HTTP/1.", (keepalive) ? "1" : "1", " 404 Not found\r\n" \
	RTMPT_SERVER__COMMON_HEADERS(keepalive) \
	"Content-Type: text/plain\r\n" \
	"Content-Length: 0\r\n"

#define RTMPT_SERVER__400_HEADERS(keepalive) \
	"HTTP/1.", (keepalive) ? "1" : "1", " 400 Bad Request\r\n" \
	RTMPT_SERVER__COMMON_HEADERS(keepalive) \
	"Content-Type: text/plain\r\n" \
	"Content-Length: 0\r\n"


using namespace M;

namespace Moment {

namespace {
LogGroup libMary_logGroup_rtmpt ("rtmpt", LogLevel::I);
}

Sender::Frontend const RtmptServer::sender_frontend = {
    NULL, // sendStateChanged
    senderClosed
};

RtmpConnection::Backend const RtmptServer::rtmp_conn_backend = {
    rtmpClosed
};

mt_rev (11.06.18)
mt_async void
RtmptServer::RtmptSender::sendMessage (Sender::MessageEntry * const mt_nonnull msg_entry,
				       bool const do_flush)
{
//    logD (rtmpt, _func, "<", fmt_hex, (UintPtr) this, "> ", fmt_def, "msg_entry: 0x", fmt_hex, (UintPtr) msg_entry);

    sender_mutex.lock ();

    nonflushed_msg_list.append (msg_entry);

  // Counting length of new data.

    switch (msg_entry->type) {
	case Sender::MessageEntry::Pages: {
	    Sender::MessageEntry_Pages * const msg_pages =
		    static_cast <Sender::MessageEntry_Pages*> (msg_entry);
	    nonflushed_data_len += msg_pages->getTotalMsgLen();
	} break;
	default:
	    unreachable ();
    }

    if (do_flush)
	doFlush ();

    sender_mutex.unlock ();
}

mt_mutex (mutex) void
RtmptServer::RtmptSender::doFlush ()
{
    pending_msg_list.stealAppend (nonflushed_msg_list.getFirst(), nonflushed_msg_list.getLast());
    pending_data_len += nonflushed_data_len;

    nonflushed_msg_list.clear();
    nonflushed_data_len = 0;
}

mt_rev (11.06.18)
mt_async void
RtmptServer::RtmptSender::flush ()
{
    logD (rtmpt, _func, "<", fmt_hex, (UintPtr) this, "> ", fmt_def, "nonflushed_data_len: ", nonflushed_data_len);

    sender_mutex.lock ();
    doFlush ();
    sender_mutex.unlock ();
}

// RtmptSender may be accessed by 'rtmp_conn' only, which in turn lists
// RmtptSession as its coderef container, thus prevening RtmptSender from being
// destroyed as well.
mt_rev (11.06.18)
mt_async void
RtmptServer::RtmptSender::closeAfterFlush ()
{
    sender_mutex.lock ();

    close_after_flush = true;

#if 0
// Unnecessary here (+ wrong).
    if (pending_data_len == 0) {
	CodeRef rtmpt_server_ref;
	if (session->weak_rtmpt_server.isValid()) {
	    rtmpt_server_ref = session->weak_rtmpt_server;
	    if (!rtmpt_server_ref)
		return;
	}
	RtmptServer * const self = session->unsafe_rtmpt_server;

	self->destroyRtmptSession (this);
    }
#endif

    sender_mutex.unlock ();
}

mt_rev (11.06.18)
mt_mutex (sender_mutex) void
RtmptServer::RtmptSender::sendPendingData (Sender * const mt_nonnull sender)
{
    logD (rtmpt, _func);

    MessageList::iter iter (pending_msg_list);
    while (!pending_msg_list.iter_done (iter)) {
	MessageEntry * const msg_entry = pending_msg_list.iter_next (iter);
//	logD (rtmpt, _func, "msg_entry: 0x", fmt_hex, (UintPtr) msg_entry);
	sender->sendMessage (msg_entry);
    }

    pending_msg_list.clear ();
    pending_data_len = 0;
}

mt_rev (11.06.18)
mt_async
RtmptServer::RtmptSender::~RtmptSender ()
{
    sender_mutex.lock ();

    {
	MessageList::iter iter (nonflushed_msg_list);
	while (!nonflushed_msg_list.iter_done (iter)) {
	    MessageEntry * const msg_entry = nonflushed_msg_list.iter_next (iter);
	    Sender::deleteMessageEntry (msg_entry);
	}
    }

    {
	MessageList::iter iter (pending_msg_list);
	while (!pending_msg_list.iter_done (iter)) {
	    MessageEntry * const msg_entry = pending_msg_list.iter_next (iter);
	    Sender::deleteMessageEntry (msg_entry);
	}
    }

    sender_mutex.unlock ();
}


RtmptServer::RtmptSession::RtmptSession (RtmptServer * const rtmpt_server,
					 Timers      * const timers,
					 PagePool    * const page_pool)
    : valid (true),
      session_id (0),
      weak_rtmpt_server (rtmpt_server),
      unsafe_rtmpt_server (rtmpt_server),
      rtmp_conn (this /* coderef_containter */, timers, page_pool),
      last_msg_time (0),
      session_keepalive_timer (NULL)
{
//    logD_ (_func, "0x", fmt_hex, (UintPtr) this);
}

RtmptServer::RtmptSession::~RtmptSession ()
{
//    logD_ (_func, "0x", fmt_hex, (UintPtr) this);
}

void
RtmptServer::sessionKeepaliveTimerTick (void * const _session)
{
//    logD_ (_func_);

    RtmptSession * const session = static_cast <RtmptSession*> (_session);

    CodeRef rtmpt_server_ref;
    if (session->weak_rtmpt_server.isValid()) {
	rtmpt_server_ref = session->weak_rtmpt_server;
	if (!rtmpt_server_ref)
	    return;
    }
    RtmptServer * const self = session->unsafe_rtmpt_server;

    self->mutex.lock ();
    if (!session->valid) {
	self->mutex.unlock ();
	return;
    }

    Time const cur_time = getTime();
    if (cur_time >= session->last_msg_time &&
	cur_time - session->last_msg_time >= self->session_keepalive_timeout)
    {
	logE_ (_func, "RTMPT session timeout");
	self->destroyRtmptSession (session);
    }

    self->mutex.unlock ();
}

mt_rev (11.06.18)
mt_mutex (mutex) void
RtmptServer::destroyRtmptSession (RtmptSession * const mt_nonnull session)
{
//    logD_ (_func, "session 0x", fmt_hex, (UintPtr) session);

    if (!session->valid) {
	logD (rtmpt, _func, "session is invalid already");
	return;
    }
    session->valid = false;

    if (session->session_keepalive_timer) {
//	logD_ (_func, "deleting session_keepalive_timer");
	timers->deleteTimer (session->session_keepalive_timer);
	session->session_keepalive_timer = NULL;
    }

    session->rtmp_conn.close_noBackendCb ();
    // Last unref.
    session_map.remove (session->session_map_entry);
}

mt_rev (11.06.18)
mt_mutex (mutex) void
RtmptServer::destroyRtmptConnection (RtmptConnection * const mt_nonnull rtmpt_conn)
{
    if (!rtmpt_conn->valid) {
	return;
    }
    rtmpt_conn->valid = false;

    // TODO Destroy conn_keepalive_timer

    conn_list.remove (rtmpt_conn);
    rtmpt_conn->unref ();
}

void
RtmptServer::senderClosed (Exception * const /* exc_ */,
			   void      * const _rtmpt_conn)
{
    RtmptConnection * const rtmpt_conn = static_cast <RtmptConnection*> (_rtmpt_conn);

    CodeRef rtmpt_server_ref;
    if (rtmpt_conn->weak_rtmpt_server.isValid()) {
	rtmpt_server_ref = rtmpt_conn->weak_rtmpt_server;
	if (!rtmpt_server_ref)
	    return;
    }
    RtmptServer * const self = rtmpt_conn->unsafe_rtmpt_server;

    self->frontend.call (self->frontend->closed, /*(*/ rtmpt_conn->conn_cb_data /*)*/);

// Too early to close the connection. It is still in use.
// In particular, it is very likely that processInput() is yet to be called
// for this connection.
//    rtmpt_conn->conn->close ();

    self->mutex.lock ();
    logD (rtmpt, _func, "calling destroyRtmptConnection");
    self->destroyRtmptConnection (rtmpt_conn);
    self->mutex.unlock ();
}

mt_rev (11.06.18)
mt_async void
RtmptServer::rtmpClosed (void * const _session)
{
    RtmptSession * const session = static_cast <RtmptSession*> (_session);

    CodeRef rtmpt_server_ref;
    if (session->weak_rtmpt_server.isValid()) {
	rtmpt_server_ref = session->weak_rtmpt_server;
	if (!rtmpt_server_ref)
	    return;
    }
    RtmptServer * const self = session->unsafe_rtmpt_server;

    self->mutex.lock ();
    if (!session->valid) {
	self->mutex.unlock ();
	return;
    }
    session->valid = false;

//    logD_ (_func, "session 0x", fmt_hex, (UintPtr) session);

    if (session->session_keepalive_timer) {
//	logD_ (_func, "deleting session_keepalive_timer");
	self->timers->deleteTimer (session->session_keepalive_timer);
	session->session_keepalive_timer = NULL;
    }

    // Last unref.
    self->session_map.remove (session->session_map_entry);
    self->mutex.unlock ();
}

mt_rev (11.06.18)
mt_mutex (mutex) void
RtmptServer::sendDataInReply (Sender       * const mt_nonnull conn_sender,
			      RtmptSession * const mt_nonnull session)
{
    logD (rtmpt, _func, "<", fmt_hex, (UintPtr) session, ":", (UintPtr) &session->rtmpt_sender, ">");

    session->rtmpt_sender.sender_mutex.lock ();

    RTMPT_SERVER__HEADERS_DATE
    conn_sender->send (
	    page_pool,
	    false /* do_flush */,
	    RTMPT_SERVER__FCS_OK_HEADERS(!no_keepalive_conns)
	    "Content-Length: ", 1 /* idle interval */ + session->rtmpt_sender.pending_data_len, "\r\n"
	    "\r\n",
	    // TODO Variable idle intervals.
	    "\x09");
    logD (rtmpt, _func, "pending data length: ", session->rtmpt_sender.pending_data_len);

    session->rtmpt_sender.sendPendingData (conn_sender);
    conn_sender->flush ();

    bool destroy_session = false;
    if (session->rtmpt_sender.close_after_flush)
	destroy_session = true;

    session->rtmpt_sender.sender_mutex.unlock ();

    // If close after flush has been requested for session->rtmpt_sender, then
    // virtual RTMP connection should be closed, hence we're destroying the session.
    if (destroy_session) {
	logD (rtmpt, _func, "calling destroyRtmptSession()");
	destroyRtmptSession (session);
    }
}

mt_rev (11.06.18)
mt_mutex (mutex) void
RtmptServer::doOpen (Sender * const mt_nonnull conn_sender,
		     IpAddress const &client_addr)
{
    logD (rtmpt, _func_);

    Ref<RtmptSession> const session = grab (new RtmptSession (this, timers, page_pool));

    session->session_id = session_id_counter;
    ++session_id_counter;

    session->rtmp_conn.setBackend (
	    Cb<RtmpConnection::Backend> (
		    &rtmp_conn_backend,
		    session,
		    session /* TODO Coderef container may be null, because session contains rtmp_conn? */));
    session->rtmp_conn.setSender (&session->rtmpt_sender);
    session->rtmp_conn.startServer ();

    session->session_map_entry = session_map.add (session);

    {
	Time const timeout = session_keepalive_timeout >= 10 ? 10 : session_keepalive_timeout;
//	logD_ (_func, "session_keepalive_timer period: ", timeout);
	// Checking for session timeout at least each 10 seconds.
	session->session_keepalive_timer = timers->addTimer (sessionKeepaliveTimerTick,
							     session,
							     session /* coderef_container */,
							     timeout,
							     true /* periodical */);
    }

    RTMPT_SERVER__HEADERS_DATE
    conn_sender->send (
	    page_pool,
	    true /* do_flush */,
	    RTMPT_SERVER__FCS_OK_HEADERS(!no_keepalive_conns)
	    "Content-Length: ", toString (Memory(), session->session_id) + 1 /* for \n */, "\r\n"
	    "\r\n",
	    session->session_id,
	    "\n");

    if (frontend && frontend->clientConnected)
	frontend.call (frontend->clientConnected, /*(*/ &session->rtmp_conn, client_addr /*)*/);
}

mt_rev (11.06.18)
mt_mutex (mutex) Ref<RtmptServer::RtmptSession>
RtmptServer::doSend (Sender * const mt_nonnull conn_sender,
		     Uint32   const session_id)
{
    SessionMap::Entry session_entry = session_map.lookup (session_id);
    if (session_entry.isNull()) {
	logD_ (_func, "Session not found: ", session_id);
	RTMPT_SERVER__HEADERS_DATE
	conn_sender->send (
		page_pool,
		true /* do_flush */,
		RTMPT_SERVER__404_HEADERS(!no_keepalive_conns)
		"\r\n");
	return NULL;
    }

    Ref<RtmptSession> const session = session_entry.getData();
    session->last_msg_time = getTime();

    sendDataInReply (conn_sender, session);
    return session;
}

mt_rev (11.06.18)
mt_mutex (mutex) void
RtmptServer::doIdle (Sender * const mt_nonnull conn_sender,
		     Uint32   const session_id)
{
    SessionMap::Entry session_entry = session_map.lookup (session_id);
    if (session_entry.isNull()) {
	logD_ (_func, "Session not found: ", session_id);
	RTMPT_SERVER__HEADERS_DATE
	conn_sender->send (
		page_pool,
		true /* do_flush */,
		RTMPT_SERVER__404_HEADERS(!no_keepalive_conns)
		"\r\n");
	return;
    }

    Ref<RtmptSession> const &session = session_entry.getData();
    session->last_msg_time = getTime();
    sendDataInReply (conn_sender, session);
}

mt_rev (11.06.18)
mt_mutex (mutex) void
RtmptServer::doClose (Sender * const mt_nonnull conn_sender,
		      Uint32   const session_id)
{
    SessionMap::Entry session_entry = session_map.lookup (session_id);
    if (session_entry.isNull()) {
	logD_ (_func, "Session not found: ", session_id);
	RTMPT_SERVER__HEADERS_DATE
	conn_sender->send (
		page_pool,
		true /* do_flush */,
		RTMPT_SERVER__404_HEADERS(!no_keepalive_conns)
		"\r\n");
	return;
    }

    // TODO FIXME Avoid closing a session while we're receiving message body
    //            from another request for the same session (cur_req_session).
    Ref<RtmptSession> const &session = session_entry.getData();
    logD (rtmpt, _func, "calling destroyRtmptSession()");
    destroyRtmptSession (session);

    RTMPT_SERVER__HEADERS_DATE
    conn_sender->send (
	    page_pool,
	    true /* do_flush */,
	    RTMPT_SERVER__OK_HEADERS(!no_keepalive_conns)
	    "Content-Type: text/plain\r\n"
	    "Content-Length: 0\r\n"
	    "\r\n");
}

HttpServer::Frontend const RtmptServer::http_frontend = {
    httpRequest,
    httpMessageBody,
    httpClosed
};

mt_rev (11.06.18)
mt_async void
RtmptServer::httpRequest (HttpRequest * const req,
			  void        * const _rtmpt_conn)
{
    RtmptConnection * const rtmpt_conn = static_cast <RtmptConnection*> (_rtmpt_conn);

    CodeRef rtmpt_server_ref;
    if (rtmpt_conn->weak_rtmpt_server.isValid()) {
	rtmpt_server_ref = rtmpt_conn->weak_rtmpt_server;
	if (!rtmpt_server_ref)
	    return;
    }
    RtmptServer * const self = rtmpt_conn->unsafe_rtmpt_server;

    self->mutex.lock ();

    logD (rtmpt, _func_);

    rtmpt_conn->cur_req_session = NULL;

    ConstMemory const command = req->getPath (0);
    logD (rtmpt, _func, "request: ", req->getRequestLine());
    logD (rtmpt, _func, "command: ", command);
    if (equal (command, "send")) {
	Uint32 const session_id = strToUlong (req->getPath (1));
	Ref<RtmptSession> const session = self->doSend (&rtmpt_conn->conn_sender, session_id);
	if (session) {
	    // Message body will be directed into the specified session
	    // in httpMessageBody().
	    rtmpt_conn->cur_req_session = session;
	}
    } else
    if (equal (command, "idle")) {
	Uint32 const session_id = strToUlong (req->getPath (1));
	self->doIdle (&rtmpt_conn->conn_sender, session_id);
    } else
    if (equal (command, "open")) {
	self->doOpen (&rtmpt_conn->conn_sender, req->getClientAddress());
    } else
    if (equal (command, "close")) {
	Uint32 const session_id = strToUlong (req->getPath (1));
	self->doClose (&rtmpt_conn->conn_sender, session_id);
    } else {
	if (!equal (command, "fcs"))
	    logW_ (_func, "uknown command: ", command);

	RTMPT_SERVER__HEADERS_DATE
	rtmpt_conn->conn_sender.send (
		self->page_pool,
		true /* do_flush */,
		RTMPT_SERVER__400_HEADERS (!self->no_keepalive_conns)
		"\r\n");
    }

    self->mutex.unlock ();

    if (self->no_keepalive_conns)
	rtmpt_conn->conn_sender.closeAfterFlush ();
}

mt_rev (11.06.18)
mt_async void
RtmptServer::httpMessageBody (HttpRequest * const mt_nonnull /* req */,
			      Memory        const &mem,
			      bool          const /* end_of_request */,
			      Size        * const mt_nonnull ret_accepted,
			      void        * const  _rtmpt_conn)
{
    RtmptConnection * const rtmpt_conn = static_cast <RtmptConnection*> (_rtmpt_conn);

    CodeRef rtmpt_server_ref;
    if (rtmpt_conn->weak_rtmpt_server.isValid()) {
	rtmpt_server_ref = rtmpt_conn->weak_rtmpt_server;
	if (!rtmpt_server_ref)
	    return;
    }
    RtmptServer * const self = rtmpt_conn->unsafe_rtmpt_server;

    logD (rtmpt, _func);
    if (logLevelOn (rtmpt, LogLevel::D)) {
        logLock ();
	hexdump (logs, mem);
        logUnlock ();
    }

    self->mutex.lock ();

    if (!rtmpt_conn->cur_req_session) {
	self->mutex.unlock ();
	*ret_accepted = mem.len();
//	logD_ (_func, "!cur_req_session");
	return;
    }

  // Processing message body of a "/send" request.

    Cb<Receiver::Frontend> const rcv_fe = rtmpt_conn->cur_req_session->rtmp_conn.getReceiverFrontend ();
    assert (rcv_fe->processInput);
    Size accepted;
    Receiver::ProcessInputResult res;
    if (!rcv_fe.call_ret<Receiver::ProcessInputResult> (&res, rcv_fe->processInput, /*(*/ mem, &accepted /*)*/)) {
	logE_ (_func, "rcv_fe gone");

	self->destroyRtmptSession (rtmpt_conn->cur_req_session);
	rtmpt_conn->cur_req_session = NULL;

	self->mutex.unlock ();

	*ret_accepted = mem.len();
	return;
    }

    // TODO InputBlocked - ?
    if (    res != Receiver::ProcessInputResult::Normal
	 && res != Receiver::ProcessInputResult::Again)
    {
	logE_ (_func, "failed to parse RTMP data: ", toString (res));

	self->destroyRtmptSession (rtmpt_conn->cur_req_session);
	rtmpt_conn->cur_req_session = NULL;

	self->mutex.unlock ();

	*ret_accepted = mem.len();
	return;
    }

    self->mutex.unlock ();

    *ret_accepted = accepted;

//    logD_ (_func, "done");
}

mt_rev (11.06.18)
mt_async void
RtmptServer::httpClosed (Exception * const exc,
			 void      * const _rtmpt_conn)
{
    RtmptConnection * const rtmpt_conn = static_cast <RtmptConnection*> (_rtmpt_conn);

    CodeRef rtmpt_server_ref;
    if (rtmpt_conn->weak_rtmpt_server.isValid()) {
	rtmpt_server_ref = rtmpt_conn->weak_rtmpt_server;
	if (!rtmpt_server_ref)
	    return;
    }
    RtmptServer * const self = rtmpt_conn->unsafe_rtmpt_server;

    if (exc)
	logE_ (_func, "exception: ", exc->toString());

    logD (rtmpt, _func, "0x", fmt_hex, (UintPtr) rtmpt_conn);

    // TODO Destroy conn_keepalive_timer

    self->frontend.call (self->frontend->closed, /*(*/ rtmpt_conn->conn_cb_data /*)*/);

    self->mutex.lock ();
//    logD_ (_func, "--- rtmpt_conn refcount: ", rtmpt_conn->getRefCount());
    self->destroyRtmptConnection (rtmpt_conn);
    self->mutex.unlock ();
}

HttpService::HttpHandler const RtmptServer::http_handler = {
    service_httpRequest,
    service_httpMessageBody
};

// TODO Code duplication with httpRequest()
Result
RtmptServer::service_httpRequest (HttpRequest   * const mt_nonnull req,
				  Sender        * const mt_nonnull conn_sender,
				  Memory const  & /* msg_body */,
				  void         ** const mt_nonnull ret_msg_data,
				  void          * const _self)
{
    RtmptServer * const self = static_cast <RtmptServer*> (_self);

    self->mutex.lock ();

    ConstMemory const command = req->getPath (0);
    logD (rtmpt, _func, "request: ", req->getRequestLine());
    if (equal (command, "send")) {
	Uint32 const session_id = strToUlong (req->getPath (1));
	Ref<RtmptSession> session = self->doSend (conn_sender, session_id);
	*ret_msg_data = session;
	session.setNoUnref ((RtmptSession*) NULL);
    } else
    if (equal (command, "idle")) {
	Uint32 const session_id = strToUlong (req->getPath (1));
	self->doIdle (conn_sender, session_id);
    } else
    if (equal (command, "open")) {
	self->doOpen (conn_sender, req->getClientAddress());
    } else
    if (equal (command, "close")) {
	Uint32 const session_id = strToUlong (req->getPath (1));
	self->doClose (conn_sender, session_id);
    } else {
	if (!equal (command, "fcs"))
	    logW_ (_func, "uknown command: ", command);

	RTMPT_SERVER__HEADERS_DATE
	conn_sender->send (
		self->page_pool,
		true /* do_flush */,
		RTMPT_SERVER__400_HEADERS (!self->no_keepalive_conns)
		"\r\n");
    }

    self->mutex.unlock ();

    if (self->no_keepalive_conns)
	conn_sender->closeAfterFlush ();

    return Result::Success;
}

// TODO Code duplication with httpMessaggeBody()
Result
RtmptServer::service_httpMessageBody (HttpRequest  * const mt_nonnull req,
				      Sender       * const mt_nonnull conn_sender,
				      Memory const &mem,
				      bool           const end_of_request,
				      Size         * const mt_nonnull ret_accepted,
				      void         * const _session,
				      void         * const _self)
{
    RtmptServer * const self = static_cast <RtmptServer*> (_self);
    RtmptSession * const session = static_cast <RtmptSession*> (_session);

  {
    if (!session) {
	*ret_accepted = mem.len();
	return Result::Success;
    }

    logD (rtmpt, _func);
    if (logLevelOn (rtmpt, LogLevel::D)) {
        logLock ();
	hexdump (logs, mem);
        logUnlock ();
    }

    self->mutex.lock ();

    if (!session->valid) {
	self->mutex.unlock ();
	*ret_accepted = mem.len();
	goto _return;
    }

  // Processing message body of a "/send" request.

    Cb<Receiver::Frontend> const rcv_fe = session->rtmp_conn.getReceiverFrontend ();
    assert (rcv_fe->processInput);
    Size accepted;
    Receiver::ProcessInputResult res;
    if (!rcv_fe.call_ret<Receiver::ProcessInputResult> (&res, rcv_fe->processInput, /*(*/ mem, &accepted /*)*/)) {
	logE_ (_func, "rcv_fe gone");

	self->destroyRtmptSession (session);
	self->mutex.unlock ();

	*ret_accepted = mem.len();
	goto _return;
    }

    // TODO InputBlocked - ?
    if (    res != Receiver::ProcessInputResult::Normal
	 && res != Receiver::ProcessInputResult::Again)
    {
//	logE_ (_func, "failed to parse RTMP data: ", toString (res));

	self->destroyRtmptSession (session);
	self->mutex.unlock ();

	*ret_accepted = mem.len();
	goto _return;
    }

    self->mutex.unlock ();

    *ret_accepted = accepted;
  }

_return:
    if (end_of_request)
	session->unref ();

//    logD_ (_func, "done");
    return Result::Success;
}

mt_rev (11.06.18)
mt_async void
RtmptServer::addConnection (Connection              * const mt_nonnull conn,
			    DependentCodeReferenced * const mt_nonnull dep_code_referenced,
			    IpAddress const         &client_addr,
			    void                    * const conn_cb_data,
			    VirtReferenced          * const ref_data)
{
    RtmptConnection * const rtmpt_conn = new RtmptConnection;
    // This is a hack. What could be done with it? (conn init callback?)
    dep_code_referenced->setCoderefContainer (rtmpt_conn);

    rtmpt_conn->weak_rtmpt_server = this;
    rtmpt_conn->unsafe_rtmpt_server = this;
    rtmpt_conn->conn = conn;
    rtmpt_conn->conn_cb_data = conn_cb_data;
    rtmpt_conn->ref_data = ref_data;
    rtmpt_conn->conn_sender.setFrontend (CbDesc<Sender::Frontend> (&sender_frontend, rtmpt_conn, rtmpt_conn));
    rtmpt_conn->conn_sender.setConnection (conn);
    rtmpt_conn->conn_receiver.setConnection (conn);
    rtmpt_conn->conn_receiver.setFrontend (rtmpt_conn->http_server.getReceiverFrontend ());

    rtmpt_conn->http_server.init (client_addr);
    rtmpt_conn->http_server.setSender (&rtmpt_conn->conn_sender, page_pool);
    rtmpt_conn->http_server.setFrontend (Cb<HttpServer::Frontend> (&http_frontend, rtmpt_conn, rtmpt_conn /* coderef_container */));

    mutex.lock ();
    conn_list.append (rtmpt_conn);
    mutex.unlock ();

//    logD_ (_func, "new rtmpt_conn refcount: ", rtmpt_conn->getRefCount());
}

void
RtmptServer::attachToHttpService (HttpService * const http_service,
				  ConstMemory   const path)
{
    ConstMemory const paths [] = { "send", "idle", "open", "close", "fcs" };
    Size const num_paths = sizeof (paths) / sizeof (ConstMemory);

    for (unsigned i = 0; i < num_paths; ++i) {
	http_service->addHttpHandler (
		CbDesc<HttpService::HttpHandler> (
			&http_handler, this, /* this */ NULL /* There's only a single static instance of RtmptServer anyway*/),
		paths [i],
		false /* preassembly */,
		0     /* preassembly_limit */,
		false /* parse_body_params */);
    }
}

RtmptServer::RtmptServer (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container),
      timers (NULL),
      page_pool (NULL),
      session_keepalive_timeout (30),
      no_keepalive_conns (false),
      session_id_counter (1)
{
}

mt_rev (11.06.18)
RtmptServer::~RtmptServer ()
{
    mutex.lock ();

    {
	SessionMap::Iterator iter (session_map);
	while (!iter.done ()) {
	    Ref<RtmptSession> &session = iter.next ().getData();
	    destroyRtmptSession (session);
	}
    }

    {
	ConnectionList::iter iter (conn_list);
	while (!conn_list.iter_done (iter)) {
	    RtmptConnection * const rtmpt_conn = conn_list.iter_next (iter);
	    destroyRtmptConnection (rtmpt_conn);
	}
    }

    mutex.unlock ();
}

}

