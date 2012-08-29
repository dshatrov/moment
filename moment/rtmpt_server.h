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
	Result (*clientConnected) (RtmpConnection  * mt_nonnull rtmp_conn,
				   IpAddress const &client_addr,
				   void            *cb_data);

	void (*closed) (void *conn_cb_data,
			void *cb_data);
    };

private:
    class RtmptSender : public Sender,
                        public DependentCodeReferenced
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

	StateMutex sender_mutex;

	mt_mutex (mutex) void doFlush ();

    public:
      mt_iface (Sender)

	mt_async void sendMessage (Sender::MessageEntry * mt_nonnull msg_entry,
				   bool do_flush = false);

	mt_async void flush ();

	mt_async void closeAfterFlush ();

        mt_async void close ();

        mt_mutex (mutex) bool isClosed_unlocked ();

        void lock ();

        void unlock ();

      mt_iface_end

	mt_mutex (sender_mutex) void sendPendingData (Sender * mt_nonnull sender);

	RtmptSender (Object *coderef_container);

	mt_async ~RtmptSender ();
    };

    class RtmptSession : public Object
    {
    public:
	bool valid;

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

	RtmptSender rtmpt_sender;
	RtmpConnection rtmp_conn;

	mt_mutex (RtmptServer::mutex) Time last_msg_time;
	mt_mutex (RtmptServer::mutex) Timers::TimerKey session_keepalive_timer;

	RtmptSession (RtmptServer *rtmpt_server);

	~RtmptSession ();
    };

    typedef RtmptSession::SessionMap_ SessionMap;

    class ConnectionList_name;

    mt_mutex (RtmptServer::mutex)
	class RtmptConnection : public Object,
				public IntrusiveListElement<ConnectionList_name>
    {
    public:
	bool valid;

	WeakCodeRef weak_rtmpt_server;
	RtmptServer *unsafe_rtmpt_server;

	mt_const Connection *conn;
	void *conn_cb_data;
	VirtRef ref_data;

	ImmediateConnectionSender conn_sender;
	ConnectionReceiver conn_receiver;
	HttpServer http_server;

	Timers::TimerKey conn_keepalive_timer;

	// TEST (uncomment)
//	RtmptSession *cur_req_session;
	Ref<RtmptSession> cur_req_session;

	RtmptConnection ()
	    : valid (true),
	      conn_sender   (this /* coderef_container */),
	      conn_receiver (this /* coderef_container */),
	      http_server   (this /* coderef_container */),
	      cur_req_session (NULL)
	{
//            logD_ (_func, " 0x", fmt_hex, (UintPtr) this);
	}

	~RtmptConnection ()
	{
//	    logD_ (_func, "0x", fmt_hex, (UintPtr) this);
	    delete conn;
	}
    };

    mt_const bool prechunking_enabled;

    mt_const Cb<Frontend> frontend;

    mt_const DataDepRef<Timers>            timers;
    mt_const DataDepRef<DeferredProcessor> deferred_processor;
    mt_const DataDepRef<PagePool>          page_pool;

    mt_const Time session_keepalive_timeout;
    mt_const bool no_keepalive_conns;

  mt_mutex (mutex)
  // {
    typedef IntrusiveList<RtmptConnection, ConnectionList_name> ConnectionList;
    ConnectionList conn_list;

    SessionMap session_map;
    // TODO IdMapper
    Uint32 session_id_counter;
  // }

    StateMutex mutex;

    static void sessionKeepaliveTimerTick (void *_session);

    mt_mutex (mutex) void destroyRtmptSession (RtmptSession * mt_nonnull session);

    mt_mutex (mutex) void destroyRtmptConnection (RtmptConnection * mt_nonnull rtmpt_conn);

  mt_iface (Sender::Frontend)
    static Sender::Frontend const sender_frontend;

// TODO
//    void senderStateChanged (Sender::SendState  send_state,
//			     void              *_rtmpt_conn);

    static void senderClosed (Exception *exc_,
			      void      *_rtmpt_conn);
  mt_iface_end

  mt_iface (RtmpConnection::Backend)
    static RtmpConnection::Backend const rtmp_conn_backend;

    static mt_async void rtmpClosed (void *_session);
  mt_iface_end

    mt_mutex (mutex) void sendDataInReply (Sender       * mt_nonnull conn_sender,
					   RtmptSession * mt_nonnull session);

    mt_mutex (mutex) void doOpen (Sender * mt_nonnull conn_sender,
				  IpAddress const &client_addr);

    mt_mutex (mutex) Ref<RtmptSession> doSend (Sender * mt_nonnull conn_sender,
					       Uint32  session_id);

    mt_mutex (mutex) void doIdle (Sender * mt_nonnull conn_sender,
				  Uint32  session_id);

    mt_mutex (mutex) void doClose (Sender * mt_nonnull conn_sender,
				   Uint32  session_id);

  mt_iface (HttpServer::Frontend)
    static HttpServer::Frontend const http_frontend;

    static mt_async void httpRequest (HttpRequest * mt_nonnull req,
				      void        *_rtmpt_conn);

    static mt_async void httpMessageBody (HttpRequest  * mt_nonnull req,
					  Memory const &mem,
					  bool          end_of_request,
					  Size         * mt_nonnull ret_accepted,
					  void         *_rtmpt_conn);

    static mt_async void httpClosed (Exception *exc,
				     void      *_rtmpt_conn);
  mt_iface_end

  mt_iface (HttpService::HttpHandler)
    static HttpService::HttpHandler const http_handler;

    static Result service_httpRequest (HttpRequest   * mt_nonnull req,
				       Sender        * mt_nonnull conn_sender,
				       Memory const  &msg_body,
				       void         ** mt_nonnull ret_msg_data,
				       void          *_self);

    static Result service_httpMessageBody (HttpRequest  * mt_nonnull req,
					   Sender       * mt_nonnull conn_sender,
					   Memory const &mem,
					   bool          end_of_request,
					   Size         * mt_nonnull ret_accepted,
					   void         *msg_data,
					   void         *_self);
  mt_iface_end

public:
    // API-разрыв: нужно делать accept на структуру данных TcpConnection... (так?).
    // _или_ большей гибкости можно добиться, если подавать Connection из кучи.
    // (?сделать его BasicReferenced?) - ни к чему...
    // => Действительно, для этих целей Connection выделяем в куче...
    // ...хотя можно было бы и в RtmptSession вписать, хвостом.

    // Takes ownership of the connection.
    // TODO Грубая несогласованность по Coderef containers.
    mt_async void addConnection (Connection              * mt_nonnull conn,
				 DependentCodeReferenced * mt_nonnull dep_code_referenced,
				 IpAddress const         &client_addr,
				 void                    *conn_cb_data,
				 VirtReferenced          *ref_data);

    // mostly mt_const
    void attachToHttpService (HttpService *http_service,
			      ConstMemory  path = ConstMemory());

    mt_const void setFrontend (Cb<Frontend> const frontend)
    {
	this->frontend = frontend;
    }

    mt_const void init (Timers   *timers,
                        DeferredProcessor *deferred_processor,
                        PagePool *page_pool,
                        Time      session_keepalive_timeout,
			bool      no_keepalive_conns,
                        bool      prechunking_enabled);

    RtmptServer (Object *coderef_container);

    ~RtmptServer ();
};

}


#endif /* __LIBMOMENT__RTMPT_SERVER__H__ */

