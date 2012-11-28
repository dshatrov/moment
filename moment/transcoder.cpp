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


#include <moment/transcoder.h>


using namespace M;

namespace Moment {

mt_const void
Transcoder::addOutputStream (VideoStream     * const out_stream,
                             ConstMemory       const chain_str,
                             TranscodingMode   const audio_mode,
                             TranscodingMode   const video_mode)
{
}

mt_const void
Transcoder::init (Timers      * const timers,
                  PagePool    * const page_pool,
                  VideoStream * const src_stream,
                  bool          const transcode_on_demand,
                  Time          const transcode_on_demand_timeout_millisec)
{
}

Transcoder::Transcoder ()
{
}

Transcoder::~Transcoder ()
{
}

}

