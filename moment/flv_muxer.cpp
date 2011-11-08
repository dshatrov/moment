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


#include <moment/flv_muxer.h>


using namespace M;

namespace Moment {

namespace {
LogGroup libMary_logGroup_flvmux ("flvmux", LogLevel::I);
}

namespace {
static Byte const flv_header [] = {
    0x46, // 'F'
    0x4c, // 'L'
    0x56, // 'V'

    0x01, // FLV version 1
    0x05, // Audio and video tags are present

    // Data offset
    0x0,
    0x0,
    0x0,
    0x9,

    // Previous tag size
    0x0,
    0x0,
    0x0,
    0x0
};
}

mt_throws Result
FlvMuxer::beginMuxing ()
{
    got_first_timestamp = false;

    PagePool::PageListHead page_list;

//    page_pool->getFillPages (&page_list, ConstMemory::forObject (flv_header));

    Size const max_header_len = RtmpConnection::MaxHeaderLen; /* FIXME Ugly */
    assert (max_header_len >= sizeof (flv_header));

    Sender::MessageEntry_Pages * const msg_pages =
	    Sender::MessageEntry_Pages::createNew (max_header_len);

    memcpy (msg_pages->getHeaderData(), flv_header, sizeof (flv_header));
    msg_pages->header_len = sizeof (flv_header);

    msg_pages->page_pool = page_pool;
    msg_pages->first_page = /* page_list.first */ NULL;
    msg_pages->msg_offset = 0;

    sender->sendMessage (msg_pages, true /* do_flush */);

    return Result::Success;
}

void
FlvMuxer::doMuxMessage (VideoStream::Message * const mt_nonnull msg,
			Byte const msg_type)
{
//    logD_ (_func, "ts 0x", fmt_hex, msg->timestamp);

    if (msg->msg_len >= (1 << 24)) {
	logE (flvmux, _func, "Message is too long (", msg->msg_len, " bytes), dropping it");
	return;
    }

    Byte const tag_header [] = {
	msg_type /* unencrypted */,

	// Data size
	(Byte) ((msg->msg_len >> 16) & 0xff),
	(Byte) ((msg->msg_len >>  8) & 0xff),
	(Byte) ((msg->msg_len >>  0) & 0xff),

	// Timestamp
	(Byte) ((msg->timestamp >> 16) & 0xff),
	(Byte) ((msg->timestamp >>  8) & 0xff),
	(Byte) ((msg->timestamp >>  0) & 0xff),

	// Extended timestamp
	(Byte) ((msg->timestamp >> 24) & 0xff),

	// Stream ID
	0,
	0,
	0
    };

    {
	Size const max_header_len = RtmpConnection::MaxHeaderLen; /* FIXME Ugly */
	assert (max_header_len >= sizeof (tag_header));

	Sender::MessageEntry_Pages * const msg_pages =
		Sender::MessageEntry_Pages::createNew (max_header_len);

	// TODO Normalize message if prechunk_size != 0

	memcpy (msg_pages->getHeaderData(), tag_header, sizeof (tag_header));
	msg_pages->header_len = sizeof (tag_header);

	if (msg->prechunk_size == 0) {
	    msg_pages->page_pool = msg->page_pool;
	    msg_pages->first_page = msg->page_list.first;
	    msg_pages->msg_offset = msg->msg_offset;

	    msg->page_pool->msgRef (msg->page_list.first);
	} else {
	    PagePool *norm_page_pool;
	    PagePool::PageListHead norm_page_list;
	    Size norm_msg_offs;
	    RtmpConnection::normalizePrechunkedData (msg->page_pool,
						     &msg->page_list,
						     msg->msg_offset,
						     msg->prechunk_size,
						     page_pool,
						     &norm_page_pool,
						     &norm_page_list,
						     &norm_msg_offs);
	    msg_pages->page_pool = norm_page_pool;
	    msg_pages->first_page = norm_page_list.first;
	    msg_pages->msg_offset = norm_msg_offs;
	}

	sender->sendMessage (msg_pages, false /* do_flush */);
    }

    {
	Size const tag_size = msg->msg_len + sizeof (tag_header); 

	Byte const tag_footer [] = {
	    (Byte) ((tag_size >> 24) & 0xff),
	    (Byte) ((tag_size >> 16) & 0xff),
	    (Byte) ((tag_size >>  8) & 0xff),
	    (Byte) ((tag_size >>  0) & 0xff)
	};

	Size const max_header_len = RtmpConnection::MaxHeaderLen; /* FIXME Ugly */
	assert (max_header_len >= sizeof (tag_footer));

	Sender::MessageEntry_Pages * const msg_pages =
		Sender::MessageEntry_Pages::createNew (max_header_len);

	memcpy (msg_pages->getHeaderData(), tag_footer, sizeof (tag_footer));
	msg_pages->header_len = sizeof (tag_footer);

	msg_pages->page_pool = NULL;
	msg_pages->first_page = NULL;
	msg_pages->msg_offset = 0;

	sender->sendMessage (msg_pages, true /* do_flush */);
    }
}

mt_throws Result
FlvMuxer::muxAudioMessage (VideoStream::AudioMessage * const mt_nonnull msg)
{
    logD (flvmux, _func, "ts: 0x", fmt_hex, msg->timestamp);
    doMuxMessage (msg, 0x8 /* audio tag */);
    return Result::Success;
}

mt_throws Result
FlvMuxer::muxVideoMessage (VideoStream::VideoMessage * const mt_nonnull msg)
{
    logD (flvmux, _func, "ts: 0x", fmt_hex, msg->timestamp);
    doMuxMessage (msg, 0x9 /* video tag */);
    return Result::Success;
}

mt_throws Result
FlvMuxer::endMuxing ()
{
    sender->closeAfterFlush ();
    return Result::Success;
}

void
FlvMuxer::reset ()
{
    got_first_timestamp = false;
}

FlvMuxer::FlvMuxer ()
    : page_pool (NULL)
{
    reset ();
}

}

