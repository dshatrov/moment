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
LogGroup libMary_logGroup_flvmux ("flvmux", LogLevel::D);
}

mt_throws Result
FlvMuxer::muxAudioMessage (VideoStream::AudioMessage * const mt_nonnull msg)
{
    logD_ (_func, "ts: 0x", fmt_hex, msg->timestamp);

    Sender::MessageEntry_Pages * const msg_pages =
	    Sender::MessageEntry_Pages::createNew (RtmpConnection::MaxHeaderLen /* FIXME Ugly */);

    // TODO Normalize message if prechunk_size != 0

    msg_pages->header_len = 0;
    msg_pages->page_pool = msg->page_pool;
    msg_pages->first_page = msg->page_list.first;
    msg_pages->msg_offset = 0;

    sender->sendMessage (msg_pages, true /* do_flush */);

    return Result::Success;
}

mt_throws Result
FlvMuxer::muxVideoMessage (VideoStream::VideoMessage * const mt_nonnull msg)
{
    logD_ (_func, "ts: 0x", fmt_hex, msg->timestamp);

    Sender::MessageEntry_Pages * const msg_pages =
	    Sender::MessageEntry_Pages::createNew (RtmpConnection::MaxHeaderLen /* FIXME Ugly */);

    // TODO Normalize message if prechunk_size != 0

    msg_pages->header_len = 0;
    msg_pages->page_pool = msg->page_pool;
    msg_pages->first_page = msg->page_list.first;
    msg_pages->msg_offset = 0;

    sender->sendMessage (msg_pages, true /* do_flush */);

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
  // TODO
}

FlvMuxer::FlvMuxer ()
{
  // TODO
}

}

