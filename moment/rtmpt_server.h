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


#ifndef __LIBMOMENT__RTMPT_SERVER__H__
#define __LIBMOMENT__RTMPT_SERVER__H__


#include <libmary/libmary.h>

#include <moment/rtmp_connection.h>


namespace Moment {

using namespace M;

class RtmptServer : public DependentCodeReferenced
{
public:
    class Frontend
    {
    public:
	// The frontend should setup RtmpConnection::Frontend or reject the connection.
	Result (*clientConnected) (RtmpConnection * mt_nonnull rtmp_conn,
				   void           *cb_data);

	void (*closed) (void *conn_cb_data,
			void *cb_data);
    };

private:
    class RtmptSender : public Sender
    {
	friend class RtmptServer;

    private:
      mt_mutex (sender_mutex)
      // {
	MessageList nonflushed_msg_list;
	Size nonflushed_data_len;

	MessageList pending_msg_list;
	Size pending_data_len;

	bool close_after_flush;
      // }

	Mutex sender_mutex;

    public:
      mt_iface (Sender)
      // {
	mt_async void sendMessage (Sender::MessageEntry * mt_nonnull msg_entry);

	mt_async void flush ();

	mt_async void closeAfterFlush ();
      // }

	mt_mutex (sender_mutex) void sendPendingData (Sender * mt_nonnull sender);

	RtmptSender ()
	    : nonflushed_data_len (0),
	      pending_data_len (0),
	      close_after_flush (false)
	{
	}

	mt_async ~RtmptSender ();
    };

    class RtmptSession : public Object
    {
    public:
	mt_const Uint32 session_id;

	typedef Map< Ref<RtmptSession>,
			 MemberExtractor< RtmptSession,
					  Uint32,
					  &RtmptSession::session_id >,
			 DirectComparator<Uint32> >
		SessionMap_;

	mt_const SessionMap_::Entry session_map_entry;

	mt_const WeakCodeRef const weak_rtmpt_server;
	mt_const RtmptServer * const unsafe_rtmpt_server;

	// Unused Timers::TimerKey session_keepalive_timer;
	// Unused Mutex session_mutex;

	RtmptSender rtmpt_sender;
	RtmpConnection rtmp_conn;

	RtmptSession (RtmptServer * const rtmpt_server,
		      Timers      * const timers,
		      PagePool    * const page_pool)
	    : weak_rtmpt_server (rtmpt_server),
	      unsafe_rtmpt_server (rtmpt_server),
	      rtmp_conn (this /* coderef_containter */, timers, page_pool)
	{
	}
    };

    typedef RtmptSession::SessionMap_ SessionMap;

    class ConnectionList_name;

    mt_mutex (RtmptServer::mutex)
	class RtmptConnection : public Object,
				public IntrusiveListElement<ConnectionList_name>
    {
    public:
	WeakCodeRef weak_rtmpt_server;
	RtmptServer *unsafe_rtmpt_server;

	Connection *conn;
	void *conn_cb_data;
	ImmediateConnectionSender conn_sender;
	ConnectionReceiver conn_receiver;
	HttpServer http_server;

	Timers::TimerKey conn_keepalive_timer;

	RtmptSession *cur_req_session;

	RtmptConnection ()
	    : conn_sender   (this /* coderef_container */),
	      conn_receiver (this /* coderef_container */),
	      http_server   (this /* coderef_container */),
	      cur_req_session (NULL)
	{
	}

	~RtmptConnection ()
	{
	    logD_ (_func, "0x", fmt_hex, (UintPtr) this);
	    delete conn;
	}
    };

    mt_const Cb<Frontend> frontend;

    mt_const Timers *timers;
    mt_const PagePool *page_pool;

  mt_mutex (mutex)
  // {
    typedef IntrusiveList<RtmptConnection, ConnectionList_name> ConnectionList;
    ConnectionList conn_list;

    SessionMap session_map;
    // TODO IdMapper
    Uint32 session_id_counter;
  // }

    Mutex mutex;

    mt_mutex (mutex) void destroyRtmptSession (RtmptSession * mt_nonnull session);

    mt_mutex (mutex) void destroyRtmptConnection (RtmptConnection * mt_nonnull rtmpt_conn);

  mt_iface (RtmpConnection::Backend)
  // {
    static RtmpConnection::Backend const rtmp_conn_backend;

    static mt_async void rtmpClosed (void *_session);
  // }

    mt_mutex (mutex) void sendDataInReply (RtmptConnection * mt_nonnull rtmpt_conn,
					   RtmptSession    * mt_nonnull session);

    mt_mutex (mutex) void doOpen (RtmptConnection * mt_nonnull rtmpt_conn);

    mt_mutex (mutex) void doSend (RtmptConnection * mt_nonnull rtmpt_conn,
				  Uint32           session_id);

    mt_mutex (mutex) void doIdle (RtmptConnection * mt_nonnull rtmpt_conn,
				  Uint32           session_id);

    mt_mutex (mutex) void doClose (RtmptConnection * mt_nonnull rtmpt_conn,
				   Uint32           session_id);

  mt_iface (HttpServer::Frontend)
  // {
    static HttpServer::Frontend const http_frontend;

    static mt_async void httpRequest (HttpRequest * mt_nonnull req,
				      void        *_rtmpt_conn);

    static mt_async void httpMessageBody (HttpRequest * mt_nonnull req,
					  Memory const &mem,
					  Size        * mt_nonnull ret_accepted,
					  void        *_rtmpt_conn);

    static mt_async void httpClosed (Exception *exc,
				     void      *_rtmpt_conn);
  // }

public:
    // API-разрыв: нужно делать accept на структуру данных TcpConnection... (так?).
    // _или_ большей гибкости можно добиться, если подавать Connection из кучи.
    // (?сделать его BasicReferenced?) - ни к чему...
    // => Действительно, для этих целей Connection выделяем в куче...
    // ...хотя можно было бы и в RtmptSession вписать, хвостом.

    // Takes ownership of the connection.
    // TODO Грубая несогласованность по Coderef containers.
    mt_async void addConnection (Connection * mt_nonnull conn,
				 DependentCodeReferenced * mt_nonnull dep_code_referenced,
				 void *conn_cb_data);

    mt_const void setFrontend (Cb<Frontend> const frontend)
    {
	this->frontend = frontend;
    }

    mt_const void setTimers (Timers * const mt_nonnull timers)
    {
	this->timers = timers;
    }

    mt_const void setPagePool (PagePool * const mt_nonnull page_pool)
    {
	this->page_pool = page_pool;
    }

    RtmptServer (Object * const coderef_container)
	: DependentCodeReferenced (coderef_container),
	  timers (NULL),
	  page_pool (NULL),
	  session_id_counter (1)
    {
    }

    ~RtmptServer ();
};

}


#endif /* __LIBMOMENT__RTMPT_SERVER__H__ */

