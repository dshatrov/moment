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


#ifndef __MOMENT__PUSH_AGENT__H__
#define __MOMENT__PUSH_AGENT__H__


#include <libmary/libmary.h>
#include <moment/push_protocol.h>
#include <moment/moment_server.h>


namespace Moment {

using namespace M;

class PushAgent : public Object
{
private:
#if 0
    class StreamTicket : public Referenced
    {
    public:
        PushAgent *push_agent;
    };
#endif

    mt_const MomentServer *moment;
    mt_const Ref<String> stream_name;

    mt_const Ref<PushConnection> push_conn;
    mt_const Ref<VideoStream> bound_stream;

//    mt_mutex (mutex) Ref<StreamTicket> cur_stream_ticket;

  mt_iface (MomentServer::VideoStreamHandler)

    static MomentServer::VideoStreamHandler moment_stream_handler;

    static void videoStreamAdded (VideoStream * mt_nonnull video_stream,
                                  ConstMemory  stream_name,
                                  void        *_self);

  mt_iface_end

#if 0
  mt_iface (VideoStream::EventHandler)

    static VideoStream::EventHandler bound_stream_handler;

    static void streamAudioMessage (VideoStream::AudioMessage * mt_nonnull msg,
                                    void                      *_self);

    static void streamVideoMessage (VideoStream::VideoMessage * mt_nonnull msg,
                                    void                      *_self);

  mt_iface_end
#endif

public:
  // TODO PushAgent забирает видеопоток из VideoStream с определённым именем
  //      и проксит его в заданный приёмник. Первый приёмник - RTMP-сервер.
  //      Протоколы для push'инга регистрируются в хэше, который есть в MomentServer.

    mt_const void init (ConstMemory   _stream_name,
                        PushProtocol * mt_nonnull push_protocol,
                        ConstMemory   uri,
                        ConstMemory   username,
                        ConstMemory   password);
};

}


#endif /* __MOMENT__PUSH_AGENT__H__ */

