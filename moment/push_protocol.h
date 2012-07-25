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


#ifndef __MOMENT__PUSH_PROTOCOL__H__
#define __MOMENT__PUSH_PROTOCOL__H__


#include <libmary/libmary.h>
#include <moment/video_stream.h>


namespace Moment {

using namespace M;

class PushConnection : public virtual Object
{
public:
};

class PushProtocol : public virtual Object
{
public:
  // 1. Connect (protocol-specific, +auth)
  // 2. Push messages: audio, video

    virtual Ref<PushConnection> connect (VideoStream *video_stream,
                                         ConstMemory  uri,
                                         ConstMemory  username,
                                         ConstMemory  password) = 0;
};

}


#endif /* __MOMENT__PUSH_PROTOCOL__H__ */

