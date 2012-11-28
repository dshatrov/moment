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


#ifndef __MOMENT__TRANSCODER_H__
#define __MOMENT__TRANSCODER_H__


#include <libmary/libmary.h>

#include <moment/video_stream.h>


namespace Moment {

using namespace M;

class Transcoder : public virtual Object
{
public:
    enum TranscodingMode
    {
        TranscodingMode_Off,
        TranscodingMode_On,
        TranscodingMode_Direct
    };

  // TODO Use pure virtual functions here.

    mt_const virtual void addOutputStream (VideoStream     *out_stream,
                                           ConstMemory      chain_str,
                                           TranscodingMode  audio_mode,
                                           TranscodingMode  video_mode) = 0;

    mt_const virtual void init (Timers      * const timers,
                                PagePool    * const page_pool,
                                VideoStream * const src_stream,
                                bool          const transcode_on_demand,
                                Time          const transcode_on_demand_timeout_millisec) = 0;

    virtual ~Transcoder ();
};

}


#endif /* __MOMENT__TRANSCODER_H__ */

