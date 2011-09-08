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

#define RTMPT_SERVER__COMMON_HEADERS \
	"Server: Moment/1.0\r\n" \
	"Date: ", ConstMemory (date_buf, date_len), "\r\n" \
	"Connection: Keep-Alive\r\n" \
	"Cache-Control: no-cache\r\n"

#define RTMPT_SERVER__OK_HEADERS \
	"HTTP/1.1 200 OK\r\n" \
	RTMPT_SERVER__COMMON_HEADERS

#define RTMPT_SERVER__FCS_OK_HEADERS \
	RTMPT_SERVER__OK_HEADERS \
	"Content-Type: application/x-fcs\r\n" \

#define RTMPT_SERVER__404_HEADERS \
	"HTTP/1.1 404 Not found\r\n" \
	RTMPT_SERVER__COMMON_HEADERS \
	"Content-Type: text/plain\r\n" \
	"Content-Length: 0\r\n"

#define RTMPT_SERVER__400_HEADERS \
	"HTTP/1.1 400 Bad Request\r\n" \
	RTMPT_SERVER__COMMON_HEADERS \
	"Content-Type: text/plain\r\n" \
	"Content-Length: 0\r\n"


using namespace M;

namespace Moment {

namespace {
LogGroup libMary_logGroup_rtmpt ("rtmpt", LogLevel::N);
}

RtmpConnection::Backend const RtmptServer::rtmp_conn_backend = {
    rtmpClosed
};

HttpServer::Frontend const RtmptServer::http_frontend = {
    httpRequest,
    httpMessageBody,
    httpClosed
};

mt_rev (11.06.18)
mt_async void
RtmptServer::RtmptSender::sendMessage (Sender::MessageEntry * const mt_nonnull msg_entry)
{
    logD (rtmpt, _func, "<", fmt_hex, (UintPtr) this, "> ", fmt_def, "msg_entry: 0x", fmt_hex, (UintPtr) msg_entry);

    sender_mutex.lock ();

    nonflushed_msg_list.append (msg_entry);

  // Counting length of new data.

    switch (msg_entry->type) {
	case Sender::MessageEntry::Pages: {
	    Sender::MessageEntry_Pages * const msg_pages =
		    static_cast <Sender::MessageEntry_Pages*> (msg_entry);

	    Size pages_data_len = 0;
	    {
		PagePool::Page *cur_page = msg_pages->first_page;
		if (cur_page) {
		    assert (msg_pages->msg_offset <= cur_page->data_len);
		    pages_data_len += cur_page->data_len - msg_pages->msg_offset;
		    cur_page = cur_page->getNextMsgPage ();
		    while (cur_page) {
			pages_data_len += cur_page->data_len;
			cur_page = cur_page->getNextMsgPage ();
		    }
		}
	    }

	    logD (rtmpt, _func, "new data len: ", msg_pages->header_len + pages_data_len);

	    nonflushed_data_len += msg_pages->header_len + pages_data_len;
	} break;
	default:
	    unreachable ();
    }

    sender_mutex.unlock ();
}

mt_rev (11.06.18)
mt_async void
RtmptServer::RtmptSender::flush ()
{
    logD (rtmpt, _func, "<", fmt_hex, (UintPtr) this, "> ", fmt_def, "nonflushed_data_len: ", nonflushed_data_len);

    sender_mutex.lock ();

    pending_msg_list.stealAppend (nonflushed_msg_list.getFirst(), nonflushed_msg_list.getLast());
    pending_data_len += nonflushed_data_len;

    nonflushed_msg_list.clear();
    nonflushed_data_len = 0;

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
	logD (rtmpt, _func, "msg_entry: 0x", fmt_hex, (UintPtr) msg_entry);
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

mt_rev (11.06.18)
mt_mutex (mutex) void
RtmptServer::destroyRtmptSession (RtmptSession * const mt_nonnull session)
{
    session->rtmp_conn.close_noBackendCb ();
    // Last unref.
    session_map.remove (session->session_map_entry);
}

mt_rev (11.06.18)
mt_mutex (mutex) void
RtmptServer::destroyRtmptConnection (RtmptConnection * const mt_nonnull rtmpt_conn)
{
    // TODO Destroy conn_keepalive_timer

    conn_list.remove (rtmpt_conn);
    rtmpt_conn->unref ();
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

    // TODO delete session keepalive timer

    self->mutex.lock ();
    // Last unref.
    self->session_map.remove (session->session_map_entry);
    self->mutex.unlock ();
}

mt_rev (11.06.18)
mt_mutex (mutex) void
RtmptServer::sendDataInReply (RtmptConnection * const mt_nonnull rtmpt_conn,
			      RtmptSession    * const mt_nonnull session)
{
    logD (rtmpt, _func, "<", fmt_hex, (UintPtr) session, ":", (UintPtr) &session->rtmpt_sender, ">");

    session->rtmpt_sender.sender_mutex.lock ();

    RTMPT_SERVER__HEADERS_DATE
    rtmpt_conn->conn_sender.send (
	    page_pool,
	    RTMPT_SERVER__FCS_OK_HEADERS
	    "Content-Length: ", 1 /* idle interval */ + session->rtmpt_sender.pending_data_len, "\r\n"
	    "\r\n",
	    // TODO Variable idle intervals.
	    "\x09");
    logD (rtmpt, _func, "pending data length: ", session->rtmpt_sender.pending_data_len);

    session->rtmpt_sender.sendPendingData (&rtmpt_conn->conn_sender);
    rtmpt_conn->conn_sender.flush ();

    if (session->rtmpt_sender.close_after_flush)
	destroyRtmptSession (session);

    session->rtmpt_sender.sender_mutex.unlock ();
}

mt_rev (11.06.18)
mt_mutex (mutex) void
RtmptServer::doOpen (RtmptConnection * const rtmpt_conn)
{
    logD (rtmpt, _func_);

    Ref<RtmptSession> const session = grab (new RtmptSession (this, timers, page_pool));

    session->session_id = session_id_counter;
    ++session_id_counter;

    session->rtmp_conn.setBackend (Cb<RtmpConnection::Backend> (&rtmp_conn_backend, session, session /* TODO Coderef container may be null, because session contains rtmp_conn? */));
    session->rtmp_conn.setSender (&session->rtmpt_sender);
    session->rtmp_conn.startServer ();

    session->session_map_entry = session_map.add (session);

    // TODO Setup session keepalive timer

    RTMPT_SERVER__HEADERS_DATE
    rtmpt_conn->conn_sender.send (
	    page_pool,
	    RTMPT_SERVER__FCS_OK_HEADERS
	    "Content-Length: ", toString (Memory(), session->session_id) + 1 /* for \n */, "\r\n"
	    "\r\n",
	    session->session_id,
	    "\n");
    rtmpt_conn->conn_sender.flush ();

    if (frontend && frontend->clientConnected)
	frontend.call (frontend->clientConnected, /*(*/ &session->rtmp_conn /*)*/);
}

mt_rev (11.06.18)
mt_mutex (mutex) void
RtmptServer::doSend (RtmptConnection * const rtmpt_conn,
		     Uint32            const session_id)
{
    SessionMap::Entry session_entry = session_map.lookup (session_id);
    if (session_entry.isNull()) {
	RTMPT_SERVER__HEADERS_DATE
	rtmpt_conn->conn_sender.send (
		page_pool,
		RTMPT_SERVER__404_HEADERS
		"\r\n");
	rtmpt_conn->conn_sender.flush ();
	return;
    }

    Ref<RtmptSession> const &session = session_entry.getData();
    // Message body will be directed into the specified session
    // in httpMessageBody().
    rtmpt_conn->cur_req_session = session;
    sendDataInReply (rtmpt_conn, session);
}

mt_rev (11.06.18)
mt_mutex (mutex) void
RtmptServer::doIdle (RtmptConnection * const rtmpt_conn,
		     Uint32            const session_id)
{
    SessionMap::Entry session_entry = session_map.lookup (session_id);
    if (session_entry.isNull()) {
	RTMPT_SERVER__HEADERS_DATE
	rtmpt_conn->conn_sender.send (
		page_pool,
		RTMPT_SERVER__404_HEADERS
		"\r\n");
	rtmpt_conn->conn_sender.flush ();
	return;
    }

    Ref<RtmptSession> const &session = session_entry.getData();
    sendDataInReply (rtmpt_conn, session);
}

mt_rev (11.06.18)
mt_mutex (mutex) void
RtmptServer::doClose (RtmptConnection * const rtmpt_conn,
		      Uint32            const session_id)
{
    SessionMap::Entry session_entry = session_map.lookup (session_id);
    if (session_entry.isNull()) {
	RTMPT_SERVER__HEADERS_DATE
	rtmpt_conn->conn_sender.send (
		page_pool,
		RTMPT_SERVER__404_HEADERS
		"\r\n");
	rtmpt_conn->conn_sender.flush ();
	return;
    }

    // TODO FIXME Avoid closing a session while we're receiving message body
    //            from another request for the same session (cur_req_session).
    Ref<RtmptSession> const &session = session_entry.getData();
    destroyRtmptSession (session);

    RTMPT_SERVER__HEADERS_DATE
    rtmpt_conn->conn_sender.send (
	    page_pool,
	    RTMPT_SERVER__OK_HEADERS
	    "Content-Type: text/plain\r\n"
	    "Content-Length: 0\r\n"
	    "\r\n");
    rtmpt_conn->conn_sender.flush ();
}

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
    logD (rtmpt, _func, "command: ", command);
    // TODO Arrange in the order of frequency (send, then idle, then open)
    if (!compare (command, "open")) {
	self->doOpen (rtmpt_conn);
    } else
    if (!compare (command, "send")) {
	Uint32 const session_id = strToUlong (req->getPath (1));
	self->doSend (rtmpt_conn, session_id);
    } else
    if (!compare (command, "idle")) {
	Uint32 const session_id = strToUlong (req->getPath (1));
	self->doIdle (rtmpt_conn, session_id);
    } else
    if (!compare (command, "close")) {
	Uint32 const session_id = strToUlong (req->getPath (1));
	self->doClose (rtmpt_conn, session_id);
    } else {
	logW_ (_func, "uknown command: ", command);

	RTMPT_SERVER__HEADERS_DATE
	rtmpt_conn->conn_sender.send (
		self->page_pool,
		RTMPT_SERVER__400_HEADERS
		"\r\n");
	rtmpt_conn->conn_sender.flush ();
    }

    self->mutex.unlock ();
}

mt_rev (11.06.18)
mt_async void
RtmptServer::httpMessageBody (HttpRequest * const mt_nonnull /* req */,
			      Memory        const &mem,
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

    logW (rtmpt, _func);
    if (logLevelOn (rtmpt, LogLevel::W))
	hexdump (logs, mem);

    self->mutex.lock ();

    if (!rtmpt_conn->cur_req_session) {
	self->mutex.unlock ();
	*ret_accepted = mem.len();
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
    self->conn_list.remove (rtmpt_conn);
//    logD_ (_func, "--- rtmpt_conn refcount: ", rtmpt_conn->getRefCount());
    self->mutex.unlock ();

    rtmpt_conn->unref ();
}

mt_rev (11.06.18)
mt_async void
RtmptServer::addConnection (Connection              * const mt_nonnull conn,
			    DependentCodeReferenced * const mt_nonnull dep_code_referenced,
			    void                    * const conn_cb_data)
{
    RtmptConnection * const rtmpt_conn = new RtmptConnection;
    // This is a hack. What could be done with it? (conn init callback?)
    dep_code_referenced->setCoderefContainer (rtmpt_conn);

    rtmpt_conn->weak_rtmpt_server = this;
    rtmpt_conn->unsafe_rtmpt_server = this;
    rtmpt_conn->conn = conn;
    rtmpt_conn->conn_cb_data = conn_cb_data,
    rtmpt_conn->conn_sender.setConnection (conn);
    rtmpt_conn->conn_receiver.setConnection (conn);
    rtmpt_conn->conn_receiver.setFrontend (rtmpt_conn->http_server.getReceiverFrontend ());
    rtmpt_conn->http_server.setSender (&rtmpt_conn->conn_sender, page_pool);
    rtmpt_conn->http_server.setFrontend (Cb<HttpServer::Frontend> (&http_frontend, rtmpt_conn, rtmpt_conn /* coderef_container */));

    mutex.lock ();
    conn_list.append (rtmpt_conn);
    mutex.unlock ();

    logD_ (_func, "new rtmpt_conn refcount: ", rtmpt_conn->getRefCount());
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

