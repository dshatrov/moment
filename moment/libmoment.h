/*  Moment Video Server - High performance media server
    Copyright (C) 2011, 2012 Dmitry Shatrov
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


#ifndef LIBMOMENT__LIBMOMENT_H__
#define LIBMOMENT__LIBMOMENT_H__


#include <libmary/libmary.h>

#include <moment/libmoment_config.h>

#include <moment/moment_types.h>

#include <moment/flv_util.h>
#include <moment/amf_encoder.h>
#include <moment/amf_decoder.h>

#include <moment/rtmp_connection.h>
#include <moment/rtmp_server.h>
#include <moment/rtmp_service.h>
#include <moment/rtmpt_server.h>
#include <moment/rtmpt_service.h>

#include <moment/video_stream.h>
#include <moment/transcoder.h>
#include <moment/test_stream_generator.h>

#include <moment/av_recorder.h>
#include <moment/av_muxer.h>
#include <moment/flv_muxer.h>

#include <moment/storage.h>
#include <moment/local_storage.h>

#include <moment/push_protocol.h>
#include <moment/push_agent.h>

#include <moment/moment_server.h>

#ifdef MOMENT_GSTREAMER
#include <moment/libmoment_gst.h>
#endif


namespace Moment {

Result configGetUint64 (MConfig::Config * mt_nonnull config,
                        ConstMemory      opt_name,
                        Uint64          * mt_nonnull ret_val,
                        Uint64           default_val);

Result configGetBoolean (MConfig::Config * mt_nonnull config,
                         ConstMemory      opt_name,
                         bool            * mt_nonnull ret_val,
                         bool             default_val);

Result configSectionGetBoolean (MConfig::Section * mt_nonnull section,
                                ConstMemory       opt_name,
                                bool             * mt_nonnull ret_val,
                                bool              default_val);

void configWarnNoEffect (ConstMemory opt_name);

}


#endif /* LIBMOMENT__LIBMOMENT_H__ */

