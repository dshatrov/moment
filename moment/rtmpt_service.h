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


#ifndef __LIBMOMENT__RTMPT_SERVICE__H__
#define __LIBMOMENT__RTMPT_SERVICE__H__


#include <libmary/libmary.h>
#include <moment/rtmp_video_service.h>
#include <moment/rtmpt_server.h>


namespace Moment {

using namespace M;

class RtmptService : public RtmpVideoService,
		     public DependentCodeReferenced
{
private:
    class ConnectionData : public Object
    {
    public:
	mt_mutex (mutex) PollGroup::PollableKey pollable_key;
    };

    mt_const PollGroup *poll_group;

    TcpServer tcp_server;
    RtmptServer rtmpt_server;

  mt_iface (RtmptServer::Frontend)
    static RtmptServer::Frontend rtmpt_server_frontend;

    static Result clientConnected (RtmpConnection  * mt_nonnull rtmp_conn,
				   IpAddress const &client_addr,
				   void            *_self);

    static void connectionClosed (void *_pollable_key,
				  void *_self);
  mt_iface_end

    bool acceptOneConnection ();

  mt_iface (TcpServer::Frontend)
    static TcpServer::Frontend tcp_server_frontend;

    static void accepted (void *_self);
  mt_iface_end

public:
    mt_throws Result init (Time session_keepalive_timeout,
			   bool no_keepalive_conns);

    mt_throws Result bind (IpAddress const &addr);

    mt_throws Result start ();

    void setTimers (Timers * const timers)
    {
	rtmpt_server.setTimers (timers);
    }

    void setPollGroup (PollGroup * const poll_group)
    {
	this->poll_group = poll_group;
    }

    void setPagePool (PagePool * const page_pool)
    {
	rtmpt_server.setPagePool (page_pool);
    }

    RtmptService (Object * const coderef_container)
	: DependentCodeReferenced (coderef_container),
	  poll_group (NULL),
	  tcp_server (coderef_container),
	  rtmpt_server (coderef_container)
    {
    }
};

}


#endif /* __LIBMOMENT__RTMPT_SERVICE__H__ */

