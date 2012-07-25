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


#include <libmary/libmary.h>
#include <moment/push_protocol.h>
#include <moment/rtmp_connection.h>


namespace Moment {

using namespace M;

class RtmpPushConnection : public PushConnection,
                           public virtual Object
{
private:
    class Session : public Object
    {
    public:
        RtmpConnection rtmp_conn;
        TcpConnection tcp_conn;
        DeferredConnectionSender conn_sender;
        ConnectionReceiver conn_receiver;

        Session ()
            : rtmp_conn     (this /* coderef_container */),
              tcp_conn      (this /* coderef_container */),
              conn_sender   (this /* coderef_container */),
              conn_receiver (this /* coderef_container */)
        {
        }
    };

    mt_const Timers *timers;
    mt_const PagePool *page_pool;

    mt_const Ref<VideoStream> video_stream;

    mt_const IpAddress   server_addr;
    mt_const Ref<String> username;
    mt_const Ref<String> password;

    mt_mutex (mutex) Ref<Session> cur_session;

    void startNewSession ();

public:
    mt_const void init (Timers      * mt_nonnull _timers,
                        PagePool    * mt_nonnull _page_pool,
                        VideoStream *_video_stream,
                        IpAddress    _server_addr,
                        ConstMemory  _username,
                        ConstMemory  _password);

    RtmpPushConnection ();
};

class RtmpPushProtocol : public PushProtocol
{
public:
  mt_iface (PushProtocol)

    // TODO Accept ServerContext or smth like that.
    Ref<PushConnection> connect (VideoStream *video_stream,
                                 ConstMemory  uri,
                                 ConstMemory  username,
                                 ConstMemory  password);

  mt_iface_end
};

}

