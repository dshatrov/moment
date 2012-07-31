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


#include <moment/push_agent.h>


using namespace M;

namespace Moment {

MomentServer::VideoStreamHandler PushAgent::moment_stream_handler = {
    videoStreamAdded
};

void
PushAgent::videoStreamAdded (VideoStream * mt_nonnull video_stream,
                             ConstMemory  stream_name,
                             void        *_self)
{
    PushAgent * const self = static_cast <PushAgent*> (_self);

    if (!equal (stream_name, self->stream_name->mem()))
        return;

    self->bound_stream->bindToStream (video_stream);
}

#if 0
VideoStream::EventHandler PushAgent::bound_stream_handler = {
    streamAudioMessage,
    streamVideoMessage,
    NULL /* rtmpCommandMessage */,
    NULL /* closed */,
    NULL /* numWatchersChanged */
};

void
PushAgent::streamAudioMessage (VideoStream::AudioMessage * const mt_nonnull msg,
                               void                      * const _self)
{
    PushAgent * const self = static_cast <PushAgent*> (_self);
    self->push_conn->pushAudioMessage (msg);
}

void
PushAgent::streamVideoMessage (VideoStream::VideoMessage * const mt_nonnull msg,
                               void                      * const _self)
{
    PushAgent * const self = static_cast <PushAgent*> (_self);
    self->push_conn->pushVideoMessage (msg);
}
#endif

mt_const void
PushAgent::init (ConstMemory    const _stream_name,
                 PushProtocol * const mt_nonnull push_protocol,
                 ConstMemory    const uri,
                 ConstMemory    const username,
                 ConstMemory    const password)
{
  // TODO
    return;

    moment = MomentServer::getInstance();
    stream_name = grab (new String (_stream_name));

    bound_stream = grab (new VideoStream);

    // TODO Get video stream by name, bind the stream, subscribe for video_stream_handler *atomically*.

    push_conn = push_protocol->connect (bound_stream, uri, username, password);

#if 0
    bound_stream->getEventInformer()->subscribe (
            CbDesc<VideoStream::EventHandler> (&bound_stream_handler, this, this));
#endif
}

}

