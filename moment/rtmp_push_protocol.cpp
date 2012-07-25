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

void
RtmpPushConnection::startNewSession ()
{
    Ref<Session> const new_session = grab (new Session);

    // TODO Setup fronend/backend/server for rtmp_conn

    new_session->rtmp_conn.init (timers,
                                 page_pool,
                                 // It is expected that there will be few push RTMP conns.
                                 // Using non-zero send delay gives negligible performance
                                 // increase in this case.
                                 0 /* send_delay_millisec */);

    // TODO Handle closed() events.

    // TODO startClient() *after* updating current session?
    new_session->rtmp_conn.startClient ();

    mutex.lock ();
    cur_session = new_session;
    mutex.unlock ();
}

mt_const void
RtmpPushConnection::init (Timers      * const mt_nonnull _timers,
                          PagePool    * const mt_nonnull _page_pool,
                          VideoStream * const _video_stream,
                          IpAddress     const _server_addr,
                          ConstMemory   const _username,
                          ConstMemory   const _password)
{
    timers = _timers;
    page_pool = _page_pool;

    video_stream = _video_stream;

    server_addr = _server_addr;
    username = grab (new String (_username));
    password = grab (new String (_password));

    startNewSession ();
}

RtmpPushConnection::RtmpPushConnection ()
{
}

Ref<PushConnection>
RtmpPushProtocol::connect (VideoStream * const video_stream,
                           ConstMemory   const uri,
                           ConstMemory   const username,
                           ConstMemory   const password)
{
  // TODO Parse 'uri', or pass address string right away.

    Ref<RtmpPushConnection> const rtmp_push_conn = grab (new RtmpPushConnection);
    rtmp_push_conn->init (NULL /* timers */, NULL /* page_pool */, video_stream, IpAddress() /* server_addr */, username, password);

    return rtmp_push_conn;
}

}

