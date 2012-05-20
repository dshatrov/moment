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


#include <libmary/module_init.h>
#include <moment/libmoment.h>


using namespace M;

namespace Moment {

namespace {

Mutex tick_mutex;

Ref<VideoStream> video_stream = NULL;
Uint64 frame_size = 2500;
Uint64 prechunk_size = 65536;

Uint64 keyframe_interval = 10;
Uint64 keyframe_counter = 0;

Uint64 start_timestamp = 0;

PagePool *page_pool = NULL;
PagePool::PageListHead page_list;

Byte *frame_buf = NULL;

bool first_frame = true;
Time timestamp_offset = 0;

Uint64 burst_width = 1;

bool use_same_pages = true;
Uint32 page_fill_counter = 0;

void frameTimerTick (void * const /* cb_data */)
{
    tick_mutex.lock ();

    for (Uint64 i = 0; i < burst_width; ++i) {
	VideoStream::VideoMessage video_msg;

	if (keyframe_counter == 0) {
	    video_msg.frame_type = VideoStream::VideoFrameType::KeyFrame;
	    keyframe_counter = keyframe_interval;
	} else {
	    video_msg.frame_type = VideoStream::VideoFrameType::InterFrame;
	    --keyframe_counter;
	}

	if (first_frame) {
	    timestamp_offset = getTimeMilliseconds();
	    video_msg.timestamp = start_timestamp;
	    first_frame = false;
	} else {
	    Time timestamp = getTimeMilliseconds();
	    if (timestamp >= timestamp_offset)
		timestamp -= timestamp_offset;
	    else
		timestamp = 0;

	    timestamp += start_timestamp;

	    video_msg.timestamp = timestamp;
	}

//	logD_ (_func, "timestamp: ", fmt_hex, video_msg.timestamp);

	video_msg.prechunk_size = prechunk_size;
	video_msg.codec_id = VideoStream::VideoCodecId::Unknown;

	PagePool::PageListHead *page_list_ptr = &page_list;
	PagePool::PageListHead tmp_page_list;
	if (!use_same_pages) {
	    page_pool->getPages (&tmp_page_list, frame_size);

	    {
		PagePool::Page *page = tmp_page_list.first;
		while (page) {
		    memset (page->getData(), (int) page_fill_counter, page->data_len);
		    page = page->getNextMsgPage();
		}
	    }

	    if (page_fill_counter < 255)
		++page_fill_counter;
	    else
		page_fill_counter = 0;

	    page_list_ptr = &tmp_page_list;
	}

	video_msg.page_pool = page_pool;
	video_msg.page_list = *page_list_ptr;
	video_msg.msg_len = frame_size;
	video_msg.msg_offset = 0;

	video_stream->fireVideoMessage (&video_msg);

	if (!use_same_pages)
	    page_pool->msgUnref (tmp_page_list.first);
    }

    tick_mutex.unlock ();
}

void modTestInit ()
{
    logD_ (_func, "Initializing mod_test");

    MomentServer * const moment = MomentServer::getInstance();
    MConfig::Config * const config = moment->getConfig();
    ServerApp * const server_app = moment->getServerApp();
    page_pool = moment->getPagePool();

    {
	ConstMemory const opt_name = "mod_test/enable";
	MConfig::Config::BooleanValue const enable = config->getBoolean (opt_name);
	if (enable == MConfig::Config::Boolean_Invalid) {
	    logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
	    return;
	}

	if (enable != MConfig::Config::Boolean_True) {
	    logI_ (_func, "Test module (mod_test) is not enabled.");
	    return;
	}
    }

    Uint64 frame_duration = 40;
    {
	ConstMemory const opt_name = "mod_test/frame_duration";
	MConfig::GetResult const res = config->getUint64_default (opt_name, &frame_duration, frame_duration);
	if (!res) {
	    logE_ (_func, "Bad value for config option ", opt_name);
	    return;
	}
    }

    {
	ConstMemory const opt_name = "mod_test/keyframe_interval";
	MConfig::GetResult const res = config->getUint64_default (opt_name, &keyframe_interval, keyframe_interval);
	if (!res) {
	    logE_ (_func, "Bad value for config option ", opt_name);
	    return;
	}
    }

    {
	ConstMemory const opt_name = "mod_test/frame_size";
	MConfig::GetResult const res = config->getUint64_default (opt_name, &frame_size, frame_size);
	if (!res) {
	    logE_ (_func, "Bad value for config option ", opt_name);
	    return;
	}
    }

    {
	ConstMemory const opt_name = "mod_test/prechunk_size";
	MConfig::GetResult const res = config->getUint64_default (opt_name, &prechunk_size, prechunk_size);
	if (!res) {
	    logE_ (_func, "Bad value for config option ", opt_name);
	    return;
	}
    }

    {
	ConstMemory const opt_name = "mod_test/start_timestamp";
	MConfig::GetResult const res = config->getUint64_default (opt_name, &start_timestamp, start_timestamp);
	if (!res) {
	    logE_ (_func, "Bad value for config option ", opt_name);
	    return;
	}
    }

    {
	ConstMemory const opt_name = "mod_test/burst_width";
	if (!config->getUint64_default (opt_name, &burst_width, burst_width)) {
	    logE_ (_func, "Bad value for config option ", opt_name);
	    return;
	}
    }

    {
	ConstMemory const opt_name = "mod_test/same_pages";
	MConfig::Config::BooleanValue const val = config->getBoolean (opt_name);
	if (val == MConfig::Config::Boolean_Invalid) {
	    logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
	    return;
	}

	if (val == MConfig::Config::Boolean_False)
	    use_same_pages = false;
	else
	    use_same_pages = true;
    }

    if (frame_size > 0) {
	frame_buf = new Byte [frame_size];
	memset (frame_buf, 0, frame_size);
    } else {
	frame_buf = NULL;
    }

    if (prechunk_size > 0) {
	RtmpConnection::PrechunkContext prechunk_ctx;
	RtmpConnection::fillPrechunkedPages (&prechunk_ctx,
					     ConstMemory (frame_buf, frame_size),
					     page_pool,
					     &page_list,
					     RtmpConnection::DefaultVideoChunkStreamId,
					     start_timestamp,
					     true /* first_chunk */);
    } else {
	page_pool->getFillPages (&page_list, ConstMemory (frame_buf, frame_size));
    }

    ConstMemory const stream_name = config->getString_default ("mod_test/stream_name", "test");

    video_stream = grab (new VideoStream);
    moment->addVideoStream (video_stream, stream_name);

    server_app->getServerContext()->getTimers()->addTimer_microseconds (
            frameTimerTick,
            NULL /* cb_data */,
            NULL /* coderef_container */,
            (Time) (frame_duration * 1000 * burst_width),
            true /* periodical */);
}

void modTestUnload ()
{
}

} // namespace {}

} // namespace Moment

namespace M {

void libMary_moduleInit ()
{
    Moment::modTestInit ();
}

void libMary_moduleUnload ()
{
    Moment::modTestUnload ();
}

} // namespace M

