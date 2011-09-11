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


#ifndef __LIBMOMENT__RTMP_SERVICE__H__
#define __LIBMOMENT__RTMP_SERVICE__H__


#include <libmary/libmary.h>
#include <moment/rtmp_video_service.h>


//#define MOMENT__RTMP_SERVICE__USE_IMMEDIATE_SENDER


namespace Moment {

using namespace M;

class RtmpService : public RtmpVideoService,
		    public DependentCodeReferenced
{
private:
    class SessionList_name;

    class ClientSession : public Object,
			  public IntrusiveListElement<SessionList_name>
    {
    public:
	bool valid;

	mt_const ServerThreadContext *thread_ctx;

	mt_const WeakCodeRef weak_rtmp_service;
	mt_const RtmpService *unsafe_rtmp_service;

	TcpConnection tcp_conn;
#ifndef MOMENT__RTMP_SERVICE__USE_IMMEDIATE_SENDER
	DeferredConnectionSender conn_sender;
#else
	ImmediateConnectionSender conn_sender;
#endif
	ConnectionReceiver conn_receiver;
	RtmpConnection rtmp_conn;
	DeferredProcessor::Registration deferred_reg;

	mt_mutex (RtmpService::mutex) PollGroup::PollableKey pollable_key;

	ClientSession (Timers   * const timers,
		       PagePool * const page_pool)
	    : thread_ctx    (NULL),
	      tcp_conn      (this /* coderef_container */),
	      conn_sender   (this /* coderef_container */),
	      conn_receiver (this /* coderef_container */, &tcp_conn),
	      rtmp_conn     (this /* coderef_container */, timers, page_pool)
	{
	}

	~ClientSession ();
    };

    typedef IntrusiveList<ClientSession, SessionList_name> SessionList;

    mt_const ServerContext *server_ctx;
    mt_const PagePool *page_pool;

    TcpServer tcp_server;

    mt_mutex (mutex) SessionList session_list;

    StateMutex mutex;

    mt_mutex (mutex) void destroySession (ClientSession *session);

    bool acceptOneConnection ();

  mt_iface (RtmpConnection::Backend)

    static RtmpConnection::Backend rtmp_conn_backend;

    static void closeRtmpConn (void * const _session);

  mt_iface_end()

  mt_iface (TcpServer::Frontend)

    static TcpServer::Frontend tcp_server_frontend;

    static void accepted (void * const _self);

  mt_iface_end()

public:
    mt_throws Result init ();

    mt_throws Result bind (IpAddress const &addr);

    mt_throws Result start ();

    void setServerContext (ServerContext * const server_ctx)
    {
	this->server_ctx = server_ctx;
    }

    void setPagePool (PagePool * const page_pool)
    {
	this->page_pool = page_pool;
    }

    RtmpService (Object * const coderef_container)
	: DependentCodeReferenced (coderef_container),
	  server_ctx (NULL),
	  page_pool (NULL),
	  tcp_server (coderef_container)
    {
    }

    ~RtmpService ();
};

}


#endif /* __LIBMOMENT__RTMP_SERVICE__H__ */

