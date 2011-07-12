/*  Moment Video Server - High performance media server
    Copyright (C) 2011 Dmitry Shatrov

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#include <moment/rtmp_connection.h>


using namespace M;

namespace Moment {

namespace {
LogGroup libMary_logGroup_chunk  ("chunk",  LogLevel::N);
LogGroup libMary_logGroup_msg    ("msg",    LogLevel::N);
LogGroup libMary_logGroup_send   ("send",   LogLevel::N);
LogGroup libMary_logGroup_writev ("writev", LogLevel::N);
LogGroup libMary_logGroup_time   ("time",   LogLevel::N);
LogGroup libMary_logGroup_close  ("rtmp_conn_close", LogLevel::D);
}

Sender::Frontend const RtmpConnection::sender_frontend = {
    // TODO
    NULL /* senderStateChanged */,
    senderClosed
};

Receiver::Frontend const RtmpConnection::receiver_frontend = {
    processInput,
    processEof,
    processError
};

RtmpConnection::ChunkStream*
RtmpConnection::getChunkStream (Uint32 const chunk_stream_id,
				bool const create)
{
    ChunkStreamTree::Node * const chunk_stream_node = chunk_stream_tree.lookup (chunk_stream_id);
    if (!chunk_stream_node) {
	if (!create)
	    return NULL;

	// TODO Max number of chunk streams.

	Ref<ChunkStream> const chunk_stream = grab (new ChunkStream);

	chunk_stream->chunk_stream_id = chunk_stream_id;
	chunk_stream->in_msg_offset = 0;
	chunk_stream->in_header_valid = false;
	chunk_stream->out_header_valid = false;
	chunk_stream_tree.add (chunk_stream);

	return chunk_stream;
    }

    return chunk_stream_node->value;
}

static inline Uint32 debugMangleTimestamp (Uint32 const timestamp)
{
//    return timestamp;
    return timestamp + 0x00ffafff; // Extended timesatmps debugging.
}

Size
RtmpConnection::fillMessageHeader (MessageDesc const * const mt_nonnull mdesc,
				   ChunkStream       * const mt_nonnull chunk_stream,
				   Byte              * const mt_nonnull header_buf)
{
    Uint32 const timestamp = debugMangleTimestamp (mdesc->timestamp);

    bool has_extended_timestamp = false;
    Uint32 extended_timestamp = 0;

    Size offs = 0;

    // Basic header (1 byte, filled below).
    offs += 1;

    bool got_header = false;
    Byte header_type = 0;

    logD (time, _func, "timestamp: 0x", fmt_hex, timestamp);

//    logD_ (_func, "chunk_stream 0x", fmt_hex, (UintPtr) chunk_stream,
//	     ", msg_len: ", fmt_def, mdesc->msg_len);

#if 0
    {
	static int counter = 100;
	if (counter >= 100) {
	    logD_ (_func, "chunk_stream 0x", fmt_hex, (UintPtr) chunk_stream,
		   ", timestamp: 0x", fmt_hex, timestamp);
	    counter = 0;
	} else {
	    ++counter;
	}
    }
#endif

    // TODO cs_hdr_comp is probably unnecessary, because all messages
    // for which we explicitly set cs_hdr_comp to 0 fit into a single 128-byte
    // chunk anyway. Is 128 bytes a minimum size for a chunk?
    if (mdesc->cs_hdr_comp &&
	chunk_stream->out_header_valid)
    {
	bool force_type0 = false;

	// TEST
//	force_type0 = true;

	if (chunk_stream->out_msg_timestamp >= 0x00ffffff) {
	    // Forcing type 0 header to be sure that all type 3 headers of
	    // this message's chunks should have extended timestamp field.
	    // This is essential for prechunking.
	    force_type0 = true;
	}

	if (!timestampGreater (chunk_stream->out_msg_timestamp, timestamp)) {
	    logW_ (_func, "!timestampGreater: ", chunk_stream->out_msg_timestamp, ", ", timestamp);
	    force_type0 = true;
	}

	if (!force_type0 &&
	    chunk_stream->out_msg_stream_id == mdesc->msg_stream_id)
	{
	    Uint32 const timestamp_delta = timestamp - chunk_stream->out_msg_timestamp;
	    if (timestamp < chunk_stream->out_msg_timestamp) {
		// This goes against RTMP rules and should never happen
		// (that's what the timestampGreater() check above is for).
		logW_ (_func, "Backwards timestamp: "
		       "new: ", timestamp, ", "
		       "old: ", chunk_stream->out_msg_timestamp);
	    }

	    if (chunk_stream->out_msg_type_id == mdesc->msg_type_id &&
		chunk_stream->out_msg_len == mdesc->msg_len)
	    {
		if (chunk_stream->out_msg_timestamp_delta == timestamp_delta &&
		    // We don't want to mix type 3 chunks and extended timestamps.
		    // (There's no well-formulated reason for this.)
		    chunk_stream->out_msg_timestamp < 0x00ffffff)
		{
		  // Type 3 header

		    got_header = true;

//		    logD_ (_func, "chunk_stream 0x", fmt_hex, (UintPtr) chunk_stream, ": "
//			   "type 3 header");

		    header_type = 3;
		    offs += Type3_HeaderLen;
		} else {
		  // Type 2 header

		    got_header = true;

//		    logD_ (_func, "chunk_stream 0x", fmt_hex, (UintPtr) chunk_stream, ": "
//			   "type 2 header, timestamp_delta: 0x", fmt_hex, timestamp_delta);

		    chunk_stream->out_msg_timestamp = timestamp;
		    chunk_stream->out_msg_timestamp_delta = timestamp_delta;

		    if (timestamp_delta >= 0x00ffffff) {
			header_buf [offs + 0] = 0xff;
			header_buf [offs + 1] = 0xff;
			header_buf [offs + 2] = 0xff;

			has_extended_timestamp = true;
			extended_timestamp = timestamp_delta;
		    } else {
			header_buf [offs + 0] = (timestamp_delta >> 16) & 0xff;
			header_buf [offs + 1] = (timestamp_delta >>  8) & 0xff;
			header_buf [offs + 2] = (timestamp_delta >>  0) & 0xff;
		    }

		    header_type = 2;
		    offs += Type2_HeaderLen;
		}
	    }

	    if (!got_header) {
	      // Type 1 header

//		logD_ (_func, "chunk_stream 0x", fmt_hex, (UintPtr) chunk_stream, ": "
//		       "type 1 header");

		got_header = true;

		chunk_stream->out_msg_timestamp = timestamp;
		chunk_stream->out_msg_timestamp_delta = timestamp_delta;
		chunk_stream->out_msg_len = mdesc->msg_len;
		chunk_stream->out_msg_type_id = mdesc->msg_type_id;

		logD (time, _func, "snd timestamp_delta: 0x", fmt_hex, timestamp_delta);

		if (timestamp_delta >= 0x00ffffff) {
		    header_buf [offs + 0] = 0xff;
		    header_buf [offs + 1] = 0xff;
		    header_buf [offs + 2] = 0xff;

		    has_extended_timestamp = true;
		    extended_timestamp = timestamp_delta;
		} else {
		    header_buf [offs + 0] = (timestamp_delta >> 16) & 0xff;
		    header_buf [offs + 1] = (timestamp_delta >>  8) & 0xff;
		    header_buf [offs + 2] = (timestamp_delta >>  0) & 0xff;
		}

		header_buf [offs + 3] = (mdesc->msg_len >> 16) & 0xff;
		header_buf [offs + 4] = (mdesc->msg_len >>  8) & 0xff;
		header_buf [offs + 5] = (mdesc->msg_len >>  0) & 0xff;

		header_buf [offs + 6] = mdesc->msg_type_id;

		header_type = 1;
		offs += Type1_HeaderLen;
	    }
	}
    }

    if (!got_header) {
      // Type 0 header

//	logD_ (_func, "chunk_stream 0x", fmt_hex, (UintPtr) chunk_stream, ": "
//	       "type 0 header");

	chunk_stream->out_header_valid = true;
	chunk_stream->out_msg_timestamp = timestamp;
	chunk_stream->out_msg_timestamp_delta = timestamp; // Somewhat weird RTMP rule.
	chunk_stream->out_msg_len = mdesc->msg_len;
	chunk_stream->out_msg_type_id = mdesc->msg_type_id;
	chunk_stream->out_msg_stream_id = mdesc->msg_stream_id;

	logD (time, _func, "snd timestamp: 0x", fmt_hex, timestamp);

	if (timestamp >= 0x00ffffff) {
	    header_buf [offs + 0] = 0xff;
	    header_buf [offs + 1] = 0xff;
	    header_buf [offs + 2] = 0xff;

	    has_extended_timestamp = true;
	    extended_timestamp = timestamp;
	} else {
	    header_buf [offs + 0] = (timestamp >> 16) & 0xff;
	    header_buf [offs + 1] = (timestamp >>  8) & 0xff;
	    header_buf [offs + 2] = (timestamp >>  0) & 0xff;
	}

	header_buf [offs + 3] = (mdesc->msg_len >> 16) & 0xff;
	header_buf [offs + 4] = (mdesc->msg_len >>  8) & 0xff;
	header_buf [offs + 5] = (mdesc->msg_len >>  0) & 0xff;

	header_buf [offs + 6] = mdesc->msg_type_id;

	// Note that msg_stream_id is not in network byte order.
	// This is a deviation from the spec.
	header_buf [offs +  7] = (mdesc->msg_stream_id >>  0) & 0xff;
	header_buf [offs +  8] = (mdesc->msg_stream_id >>  8) & 0xff;
	header_buf [offs +  9] = (mdesc->msg_stream_id >> 16) & 0xff;
	header_buf [offs + 10] = (mdesc->msg_stream_id >> 24) & 0xff;

	header_type = 0;
	offs += Type0_HeaderLen;
    }

    if (has_extended_timestamp) {
//	logD_ (_func, "extended timestamp");

	header_buf [offs + 0] = (extended_timestamp >> 24) & 0xff;
	header_buf [offs + 1] = (extended_timestamp >> 16) & 0xff;
	header_buf [offs + 2] = (extended_timestamp >>  8) & 0xff;
	header_buf [offs + 3] = (extended_timestamp >>  0) & 0xff;

	offs += 4;
    }

    // FIXME Assuming small chunk stream ids (2-63)
    header_buf [0] = (header_type << 6) | (Byte) chunk_stream->chunk_stream_id;

//    logD_ (_func, "header_type: ", header_type, ", offs: ", offs);
//    hexdump (logs, ConstMemory (header_buf, offs));

    assert (offs <= MaxHeaderLen);
    return offs;
}

void
RtmpConnection::fillPrechunkedPages (PrechunkContext        * const  prechunk_ctx,
				     ConstMemory              const &mem,
				     PagePool               * const  page_pool,
				     PagePool::PageListHead * const  page_list,
				     Uint32                   const  chunk_stream_id,
				     Uint32                          msg_timestamp,
				     bool                     const  first_chunk)
{
    msg_timestamp = debugMangleTimestamp (msg_timestamp);

    Size const prechunk_size = PrechunkSize;

    logD (chunk, _func, mem.len(), " bytes, prechunk_size: ", prechunk_size);

    Size total_filled = 0;

    // There's no need to reset 'prechunk_ctx->prechunk_offset' to 0, because
    // it is set to 0 in PrechunkContext() constructor, and we require distinct
    // contexts for distinct messages.

    while (total_filled < mem.len ()) {
	if (prechunk_ctx->prechunk_offset == 0 &&
		!(first_chunk && total_filled == 0))
	{
	    logD (chunk, _func, "chunk header, total_filled ", total_filled);

	    // TODO Large chunk stream ids.
	    assert (chunk_stream_id > 1 && chunk_stream_id < 64);

	    if (msg_timestamp < 0x00ffffff) {
		Byte const header_buf = (Byte) 0xc0 | (Byte) chunk_stream_id;
		page_pool->getFillPages (page_list, ConstMemory (&header_buf, 1));
	    } else {
		Byte header [5];
		header [0] = (Byte) 0xc0 | (Byte) chunk_stream_id;
		header [1] = (msg_timestamp >> 24) & 0xff;
		header [2] = (msg_timestamp >> 16) & 0xff;
		header [3] = (msg_timestamp >>  8) & 0xff;
		header [4] = (msg_timestamp >>  0) & 0xff;
		page_pool->getFillPages (page_list, ConstMemory (header, 5));
	    }
	}

	Size tofill;
	assert (prechunk_ctx->prechunk_offset < prechunk_size);
	if (prechunk_size - prechunk_ctx->prechunk_offset < mem.len() - total_filled)
	    tofill = prechunk_size - prechunk_ctx->prechunk_offset;
	else
	    tofill = mem.len() - total_filled;

	page_pool->getFillPages (page_list, ConstMemory (mem.mem() + total_filled, tofill));

	total_filled += tofill;

	prechunk_ctx->prechunk_offset += tofill;
	assert (prechunk_ctx->prechunk_offset <= prechunk_size);
	if (prechunk_ctx->prechunk_offset == prechunk_size)
	    prechunk_ctx->prechunk_offset = 0;
    }
}

void
RtmpConnection::sendMessage (MessageDesc const * const mt_nonnull mdesc,
			     ChunkStream       * const mt_nonnull chunk_stream,
			     ConstMemory const &mem,
			     Uint32             prechunk_size)
{
    PagePool::PageListHead page_list;
    if (prechunk_size > 0) {
	page_pool->getFillPages (&page_list, mem);
    } else {
	prechunk_size = PrechunkSize;

	PrechunkContext prechunk_ctx;
	fillPrechunkedPages (&prechunk_ctx,
			     mem,
			     page_pool,
			     &page_list,
			     chunk_stream->chunk_stream_id,
			     mdesc->timestamp,
			     true /* first_chunk */);
#if 0
	logD (chunk, _func, "prechunked 1st page:");
	if (page_list.first)
	    hexdump (ConstMemory (page_list.first->getData(), page_list.first->data_len));
#endif
    }

    // TODO put_pages or 'bool take_ownership'.
    sendMessagePages (mdesc, chunk_stream, &page_list, 0 /* msg_offset */, prechunk_size, true /* take_ownership */);
}

// TODO first_page is enough, page_list not needed
void
RtmpConnection::sendMessagePages (MessageDesc const      * const mt_nonnull mdesc,
				  ChunkStream            * const mt_nonnull chunk_stream,
				  PagePool::PageListHead * const mt_nonnull page_list,
				  Size                     const msg_offset,
				  Uint32                         prechunk_size,
				  bool                     const take_ownership)
{
    if (is_closed) {
	logD (close, _func, "0x", fmt_hex, (UintPtr) this, " is closed");
	// TODO unref pages if (bool take_ownership).
	// ^^^^ ???
	return;
    }

    Sender::MessageEntry_Pages * const msg_pages =
	    Sender::MessageEntry_Pages::createNew (MaxHeaderLen);
    msg_pages->header_len = fillMessageHeader (mdesc, chunk_stream, msg_pages->getHeaderData());
#if 0
    {
	logLock ();
	logD_unlocked_ (_func, "header:");
	hexdump (logs, ConstMemory (msg_pages->getHeaderData(), msg_pages->header_len));
	logUnlock ();
    }
#endif
    msg_pages->page_pool = page_pool;
    msg_pages->msg_offset = msg_offset;

    if (prechunk_size == 0) {
	prechunk_size = PrechunkSize;

	PrechunkContext prechunk_ctx;
	PagePool::PageListHead prechunked_pages;

	PagePool::Page *page = page_list->first;
	while (page) {
	    if (page == page_list->first) {
		fillPrechunkedPages (&prechunk_ctx,
				     page->mem().region (msg_offset),
				     page_pool,
				     &prechunked_pages,
				     chunk_stream->chunk_stream_id,
				     mdesc->timestamp,
				     true /* first_chunk */);
	    } else {
		fillPrechunkedPages (&prechunk_ctx,
				     page->mem(),
				     page_pool,
				     &prechunked_pages,
				     chunk_stream->chunk_stream_id,
				     mdesc->timestamp,
				     false /* first_chunk */);
	    }

	    page = page->getNextMsgPage();
	}

	msg_pages->first_page = prechunked_pages.first;

	if (take_ownership)
	    page_pool->msgUnref (page_list->first);
    } else {
	msg_pages->first_page = page_list->first;

	if (!take_ownership)
	    page_pool->msgRef (page_list->first);
    }

    if (prechunk_size != out_chunk_size) {
	sendSetChunkSize (prechunk_size);
	out_chunk_size = prechunk_size;
    }

    sender->sendMessage (msg_pages);
    sender->flush ();
}

void
RtmpConnection::sendRawPages (PagePool::Page * const first_page,
			      Size const msg_offset)
{
    logD (send, _func_);

    if (is_closed) {
	logD (close, _func, "0x", fmt_hex, (UintPtr) this, " is closed");
	return;
    }

    Sender::MessageEntry_Pages * const msg_pages =
	    Sender::MessageEntry_Pages::createNew (0 /* max_header_len */);
    msg_pages->header_len = 0;
    msg_pages->page_pool = page_pool;
    msg_pages->first_page = first_page;
    msg_pages->msg_offset = msg_offset;

    sender->sendMessage (msg_pages);
    sender->flush ();
}

void
RtmpConnection::resetPacket ()
{
    logD (chunk, _func_);

    conn_state = ReceiveState::BasicHeader;
    cs_id = 0;
    cs_id__fmt = CsIdFormat::Unknown;
    chunk_offset = 0;
}

void
RtmpConnection::resetMessage (ChunkStream * const mt_nonnull chunk_stream)
{
    logD (send, _func_);

    if (!chunk_stream->page_list.isEmpty ()) {
	page_pool->msgUnref (chunk_stream->page_list.first);
    }
    chunk_stream->page_list.reset ();

    chunk_stream->in_msg_offset = 0;
    chunk_stream->in_prechunk_ctx.reset ();
}

void
RtmpConnection::sendSetChunkSize (Uint32 const chunk_size)
{
    Byte msg [4];

    msg [0] = (chunk_size >> 24) & 0xff;
    msg [1] = (chunk_size >> 16) & 0xff;
    msg [2] = (chunk_size >>  8) & 0xff;
    msg [3] = (chunk_size >>  0) & 0xff;

    MessageDesc mdesc;
    mdesc.timestamp = 0;
    mdesc.msg_type_id = RtmpMessageType::SetChunkSize;
    mdesc.msg_stream_id = CommandMessageStreamId;
    mdesc.msg_len = sizeof (msg);
    mdesc.cs_hdr_comp = 0;

    // We pass 'out_chunk_size' for @prechunk_size parameter to prevent
    // sendMessagePages() from calling sendSetChunkSize() recursively.
    // This is a safe thing to do, since "set chunk size" message always fits
    // into a single chunk.
    sendMessage (&mdesc, control_chunk_stream, ConstMemory::forObject (msg), out_chunk_size /* prechunk_size */);
}

void
RtmpConnection::sendAck (Uint32 const seq)
{
    Byte msg [4];

    msg [0] = (seq >> 24) & 0xff;
    msg [1] = (seq >> 16) & 0xff;
    msg [2] = (seq >>  8) & 0xff;
    msg [3] = (seq >>  0) & 0xff;

    MessageDesc mdesc;
    mdesc.timestamp = 0;
    mdesc.msg_type_id = RtmpMessageType::Ack;
    mdesc.msg_stream_id = CommandMessageStreamId;
    mdesc.msg_len = sizeof (msg);
    mdesc.cs_hdr_comp = 0;

    sendMessage (&mdesc, control_chunk_stream, ConstMemory::forObject (msg), 0 /* prechunk_size */);
}

void
RtmpConnection::sendWindowAckSize (Uint32 const wack_size)
{
    Byte msg [4];

    msg [0] = (wack_size >> 24) & 0xff;
    msg [1] = (wack_size >> 16) & 0xff;
    msg [2] = (wack_size >>  8) & 0xff;
    msg [3] = (wack_size >>  0) & 0xff;

    MessageDesc mdesc;
    mdesc.timestamp = 0;
    mdesc.msg_type_id = RtmpMessageType::WindowAckSize;
    mdesc.msg_stream_id = CommandMessageStreamId;
    mdesc.msg_len = sizeof (msg);
    mdesc.cs_hdr_comp = 0;

    sendMessage (&mdesc, control_chunk_stream, ConstMemory::forObject (msg), 0 /* prechunk_size */);
}

void
RtmpConnection::sendSetPeerBandwidth (Uint32 const wack_size,
				      Byte   const limit_type)
{
    Byte msg [5];

    msg [0] = (wack_size >> 24) & 0xff;
    msg [1] = (wack_size >> 16) & 0xff;
    msg [2] = (wack_size >>  8) & 0xff;
    msg [3] = (wack_size >>  0) & 0xff;

    msg [4] = limit_type;

    MessageDesc mdesc;
    mdesc.timestamp = 0;
    mdesc.msg_type_id = RtmpMessageType::SetPeerBandwidth;
    mdesc.msg_stream_id = CommandMessageStreamId;
    mdesc.msg_len = sizeof (msg);
    mdesc.cs_hdr_comp = 0;

    sendMessage (&mdesc, control_chunk_stream, ConstMemory::forObject (msg), 0 /* prechunk_size */);
}

void
RtmpConnection::sendUserControl_StreamBegin (Uint32 const msg_stream_id)
{
    Byte msg [6];

    msg [0] = 0x00;
    msg [1] = 0x00;

    msg [2] = (msg_stream_id >> 24) & 0xff;
    msg [3] = (msg_stream_id >> 16) & 0xff;
    msg [4] = (msg_stream_id >>  8) & 0xff;
    msg [5] = (msg_stream_id >>  0) & 0xff;

    MessageDesc mdesc;
    mdesc.timestamp = 0;
    mdesc.msg_type_id = RtmpMessageType::UserControl;
    mdesc.msg_stream_id = msg_stream_id /* XXX CommandMessageStreamId? */;
    mdesc.msg_len = sizeof (msg);
    mdesc.cs_hdr_comp = 0;

    sendMessage (&mdesc, control_chunk_stream, ConstMemory::forObject (msg), 0 /* prechunk_size */);
}

void
RtmpConnection::sendUserControl_SetBufferLength (Uint32 const msg_stream_id,
						 Uint32 const buffer_len)
{
    Byte msg [10];

    msg [0] = 0x00;
    msg [1] = 0x03;

    msg [2] = (msg_stream_id >> 24) & 0xff;
    msg [3] = (msg_stream_id >> 16) & 0xff;
    msg [4] = (msg_stream_id >>  8) & 0xff;
    msg [5] = (msg_stream_id >>  0) & 0xff;

    msg [6] = (buffer_len >> 24) & 0xff;
    msg [7] = (buffer_len >> 16) & 0xff;
    msg [8] = (buffer_len >>  8) & 0xff;
    msg [9] = (buffer_len >>  0) & 0xff;

    MessageDesc mdesc;
    mdesc.timestamp = 0;
    mdesc.msg_type_id = RtmpMessageType::UserControl;
    mdesc.msg_stream_id = msg_stream_id; /* XXX CommandMessageStreamId? */
    mdesc.msg_len = sizeof (msg);
    mdesc.cs_hdr_comp = 0;

    sendMessage (&mdesc, control_chunk_stream, ConstMemory::forObject (msg), 0 /* prechunk_size */);
}

void
RtmpConnection::sendUserControl_StreamIsRecorded (Uint32 const msg_stream_id)
{
    Byte msg [6];

    msg [0] = 0x00;
    msg [1] = 0x04;

    msg [2] = (msg_stream_id >> 24) & 0xff;
    msg [3] = (msg_stream_id >> 16) & 0xff;
    msg [4] = (msg_stream_id >>  8) & 0xff;
    msg [5] = (msg_stream_id >>  0) & 0xff;

    MessageDesc mdesc;
    mdesc.timestamp = 0;
    mdesc.msg_type_id = RtmpMessageType::UserControl;
    mdesc.msg_stream_id = msg_stream_id; /* XXX CommandMessageStreamId? */
    mdesc.msg_len = sizeof (msg);
    mdesc.cs_hdr_comp = 0;

    sendMessage (&mdesc, control_chunk_stream, ConstMemory::forObject (msg), 0 /* prechunk_size */);
}

void
RtmpConnection::sendUserControl_PingRequest ()
{

    Byte msg [6];

    Uint32 const time = getTime (); // XXX Time in seconds?

    msg [0] = 0x00;
    msg [1] = 0x06;

    msg [2] = (time >> 24) & 0xff;
    msg [3] = (time >> 16) & 0xff;
    msg [4] = (time >>  8) & 0xff;
    msg [5] = (time >>  0) & 0xff;

    MessageDesc mdesc;
    mdesc.timestamp = 0;
    mdesc.msg_type_id = RtmpMessageType::UserControl;
    mdesc.msg_stream_id = CommandMessageStreamId;
    mdesc.msg_len = sizeof (msg);
    mdesc.cs_hdr_comp = 0;

    sendMessage (&mdesc, control_chunk_stream, ConstMemory::forObject (msg), 0 /* prechunk_size */);
}

void
RtmpConnection::sendUserControl_PingResponse (Uint32 const timestamp)
{
    Byte msg [6];

    msg [0] = 0x00;
    msg [1] = 0x07;

    msg [2] = (timestamp >> 24) & 0xff;
    msg [3] = (timestamp >> 16) & 0xff;
    msg [4] = (timestamp >>  8) & 0xff;
    msg [5] = (timestamp >>  0) & 0xff;

    MessageDesc mdesc;
    mdesc.timestamp = 0;
    mdesc.msg_type_id = RtmpMessageType::UserControl;
    mdesc.msg_stream_id = CommandMessageStreamId;
    mdesc.msg_len = sizeof (msg);
    mdesc.cs_hdr_comp = 0;

    sendMessage (&mdesc, control_chunk_stream, ConstMemory::forObject (msg), 0 /* prechunk_size */);
}

void
RtmpConnection::sendCommandMessage_AMF0 (Uint32 const msg_stream_id,
					 ConstMemory const &mem)
{
    MessageDesc mdesc;
    mdesc.timestamp = 0; // XXX Why not set real non-zero timestamp? - Возможно, при нулевом таймстемпе сообщения не оседают в буфере, а сразу обрабатываются?
    mdesc.msg_type_id = RtmpMessageType::Command_AMF0;
    mdesc.msg_stream_id = msg_stream_id;
    mdesc.msg_len = mem.len();
    mdesc.cs_hdr_comp = 0;

    sendMessage (&mdesc, data_chunk_stream, mem, 0 /* prechunk_size */);
}

void
RtmpConnection::sendConnect ()
{
    AmfAtom atoms [16];
    AmfEncoder encoder (atoms);

    encoder.addString ("connect");
    encoder.addNumber (1.0);

    {
	encoder.beginObject ();

	encoder.addFieldName ("app");
	encoder.addString ("");

	encoder.addFieldName ("flashVer");
	encoder.addString ("LNX 10,0,22,87");

	encoder.addFieldName ("tcUrl");
	encoder.addString ("");

	encoder.addFieldName ("fpad");
	encoder.addBoolean (false);

	encoder.addFieldName ("audioCodecs");
	encoder.addNumber ((double) 0x00fff /* SUPPORT_SND_ALL */);

	encoder.addFieldName ("videoCodecs");
	encoder.addNumber ((double) 0x00ff /* SUPPORT_VID_ALL */);

	encoder.endObject ();
    }

    Byte msg_buf [512];
    Size msg_len;
    if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	logE_ (_func, "encode() failed");
	unreachable ();
    }

    sendCommandMessage_AMF0 (CommandMessageStreamId, ConstMemory (msg_buf, msg_len));
    logD (send, _func, "msg_len: ", msg_len);
    if (logLevelOn (send, LogLevel::Debug))
	hexdump (ConstMemory (msg_buf, msg_len));
}

void
RtmpConnection::sendCreateStream ()
{
    AmfAtom atoms [3];
    AmfEncoder encoder (atoms);

    encoder.addString ("createStream");
    // FIXME Use saner transaction ids.
    encoder.addNumber (2.0);

    encoder.addNullObject ();

    Byte msg_buf [512];
    Size msg_len;
    if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	logE_ (_func, "encode() failed");
	unreachable ();
    }

    sendCommandMessage_AMF0 (CommandMessageStreamId, ConstMemory (msg_buf, msg_len));
}

void
RtmpConnection::sendPlay (ConstMemory const &stream_name)
{
    AmfAtom atoms [4];
    AmfEncoder encoder (atoms);

    encoder.addString ("play");
    // FIXME Use saner transaction ids.
    encoder.addNumber (3.0);
    encoder.addNullObject ();
    encoder.addString (stream_name);

    // FIXME stream_name shouldn't be too long, otherwise the message will not be sent.
    Byte msg_buf [4096];
    Size msg_len;
    if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	logE_ (_func, "encode() failed");
	// TODO Uncomment once message length limitation is resolved.
	// unreachable ();
    }

    sendCommandMessage_AMF0 (CommandMessageStreamId, ConstMemory (msg_buf, msg_len));
}

void
RtmpConnection::closeAfterFlush ()
{
    logD (close, _func, "0x", fmt_hex, (UintPtr) this);
    sender->closeAfterFlush ();
}

void
RtmpConnection::close ()
{
    logD (close, _func, "0x", fmt_hex, (UintPtr) this);
    is_closed = true;
    frontend.call (frontend->closed, /*(*/ (Exception*) NULL /*)*/);
    backend.call (backend->close);
}

void
RtmpConnection::close_noBackendCb ()
{
    logD (close, _func, "0x", fmt_hex, (UintPtr) this);
    is_closed = true;
    frontend.call (frontend->closed, /*(*/ (Exception*) NULL /*)*/);
}

void
RtmpConnection::beginPings ()
{
    if (ping_send_timer)
	return;

    ping_send_timer = timers->addTimer (pingTimerTick,
					this,
					getCoderefContainer(),
					5 * 60 /* TODO Config parameter for timeout */,
					true /* periodical */);
    ping_reply_received = true;
}

void
RtmpConnection::pingTimerTick (void * const _self)
{
    RtmpConnection * const self = static_cast <RtmpConnection*> (_self);

  // TODO Synchronization for ping_reply_received.

    if (!self->ping_reply_received) {
	logE_ (_func, "no ping reply");
	logD (close, _func, "0x", fmt_hex, (UintPtr) self, " closing");
	self->is_closed = true;
	{
	    InternalException internal_exc (InternalException::ProtocolError);
	    self->frontend.call (self->frontend->closed, /*(*/ &internal_exc /*)*/);
	}
	self->backend.call (self->backend->close);
	return;
    }

    self->ping_reply_received = false;
    self->sendUserControl_PingRequest ();
}

Result
RtmpConnection::processMessage (ChunkStream * const chunk_stream)
{
    logD (msg, _func_);

    Byte const *msg_buf = chunk_stream->page_list.first->getData();
    Size const msg_len = chunk_stream->in_msg_len;

    logD (msg, _func, "message type id: ", chunk_stream->in_msg_type_id);
    switch (chunk_stream->in_msg_type_id) {
	case RtmpMessageType::SetChunkSize: {
	    logD (msg, _func, "SetChunkSize");

	    if (msg_len < 4) {
		logE_ (_func, "SetChunkSize message is too short (", msg_len, " bytes)");
		return Result::Failure;
	    }

	    Uint32 const chunk_size = ((Uint32) msg_buf [0] << 24) |
				      ((Uint32) msg_buf [1] << 16) |
				      ((Uint32) msg_buf [2] <<  8) |
				      ((Uint32) msg_buf [3] <<  0);

	    if (chunk_size < MinChunkSize ||
		chunk_size > MaxChunkSize)
	    {
		logE_ (_func, "SetChunkSize: bad chunk size: ", chunk_size);
		return Result::Failure;
	    }

	    logD (msg, _func, "SetChunkSize: new chunk size: ", chunk_size);

	    in_chunk_size = chunk_size;
	} break;
	case RtmpMessageType::Abort: {
	    // TODO Verify that Abort is handled correctly.

	    logD (msg, _func, "Abort");

	    if (msg_len < 4) {
		logE_ (_func, "Abort message is too short (", msg_len, " bytes)");
		return Result::Failure;
	    }

	    Uint32 const chunk_stream_id = ((Uint32) msg_buf [0] << 24) |
					   ((Uint32) msg_buf [1] << 16) |
					   ((Uint32) msg_buf [2] <<  8) |
					   ((Uint32) msg_buf [3] <<  0);

	    ChunkStream * const chunk_stream = getChunkStream (chunk_stream_id, false /* create */);
	    if (!chunk_stream) {
		logE_ (_func, "Abort: stream not found: ", chunk_stream_id);
		return Result::Failure;
	    }

	    resetMessage (chunk_stream);
	} break; 
	case RtmpMessageType::Ack: {
	    logD (msg, _func, "Ack");

	    if (msg_len < 4) {
		logE_ (_func, "Ack message is too short (", msg_len, " bytes)");
		return Result::Failure;
	    }

	    Uint32 const bytes_received = ((Uint32) msg_buf [0] << 24) |
					  ((Uint32) msg_buf [1] << 16) |
					  ((Uint32) msg_buf [2] <<  8) |
					  ((Uint32) msg_buf [3] <<  0);

	    logD (msg, _func, "Ack: ", bytes_received);

	    // TODO Handle acks.
	} break;
	case RtmpMessageType::UserControl: {
	    logD (msg, _func, "UserControl");

	    return processUserControlMessage (chunk_stream);
	} break;
	case RtmpMessageType::WindowAckSize: {
	    logD (msg, _func, "WindowAckSize");

	    if (msg_len < 4) {
		logE_ (_func, "WindowAckSize message is too short (", msg_len, " bytes)");
		return Result::Failure;
	    }

	    Uint32 const wack_size = ((Uint32) msg_buf [0] << 24) |
				     ((Uint32) msg_buf [1] << 16) |
				     ((Uint32) msg_buf [2] <<  8) |
				     ((Uint32) msg_buf [3] <<  0);

	    remote_wack_size = wack_size;
	} break;
	case RtmpMessageType::SetPeerBandwidth: {
	    logD (msg, _func, "SetPeerBandwidth");

	    if (msg_len < 5) {
		logE_ (_func, "SetPeerBandwidth message is too short (", msg_len, " bytes)");
		return Result::Failure;
	    }

	    Uint32 const wack_size = ((Uint32) msg_buf [0] << 24) |
				     ((Uint32) msg_buf [1] << 16) |
				     ((Uint32) msg_buf [2] <<  8) |
				     ((Uint32) msg_buf [3] <<  0);

	    // Unused
	    // Byte const limit_type = msg_buf [4];

	    if (local_wack_size != wack_size)
		sendWindowAckSize (local_wack_size);
	} break;
	case RtmpMessageType::AudioMessage: {
	    logD (msg, _func, "AudioMessage");

	    if (frontend && frontend->audioMessage) {
		MessageInfo msg_info;
		msg_info.msg_stream_id = chunk_stream->in_msg_stream_id;
		msg_info.timestamp = chunk_stream->in_msg_timestamp;
		msg_info.prechunk_size = (prechunking_enabled ? PrechunkSize : 0);
		Result res = Result::Failure;
		frontend.call_ret<Result> (&res, frontend->audioMessage, /*(*/ &msg_info, &chunk_stream->page_list, msg_len /*)*/);
		return res;
	    }
	} break;
	case RtmpMessageType::VideoMessage: {
	    logD (msg, _func, "VideoMessage");

	    if (frontend && frontend->videoMessage) {
		MessageInfo msg_info;
		msg_info.msg_stream_id = chunk_stream->in_msg_stream_id;
		msg_info.timestamp = chunk_stream->in_msg_timestamp;
		msg_info.prechunk_size = (prechunking_enabled ? PrechunkSize : 0);
		Result res = Result::Failure;
		frontend.call_ret<Result> (&res, frontend->videoMessage, /*(*/ &msg_info, &chunk_stream->page_list, msg_len /*)*/);
		return res;
	    }
	} break;
	case RtmpMessageType::Data_AMF3: {
	    logD (msg, _func, "Data_AMF3");
	    return callCommandMessage (chunk_stream, AmfEncoding::AMF3);
	} break;
	case RtmpMessageType::Data_AMF0: {
	    logD (msg, _func, "Data_AMF0");
	    return callCommandMessage (chunk_stream, AmfEncoding::AMF0);
	} break;
	case RtmpMessageType::SharedObject_AMF3: {
	    logD (msg, _func, "SharedObject_AMF3");
	  // No-op
	} break;
	case RtmpMessageType::SharedObject_AMF0: {
	    logD (msg, _func, "SharedObject_AMF0");
	  // No-op
	} break;
	case RtmpMessageType::Command_AMF3: {
	    logD (msg, _func, "Command_AMF3");
	    return callCommandMessage (chunk_stream, AmfEncoding::AMF3);
	} break;
	case RtmpMessageType::Command_AMF0: {
	    logD (msg, _func, "Command_AMF0");
	    return callCommandMessage (chunk_stream, AmfEncoding::AMF0);
	} break;
	case RtmpMessageType::Aggregate: {
	    logD (msg, _func, "Aggregate");
	  // No-op
	} break;
	default:
	    logW_ (_func, "unknown message type: 0x", fmt_hex, chunk_stream->in_msg_type_id);
    }

    return Result::Success;
}

Result
RtmpConnection::callCommandMessage (ChunkStream * const chunk_stream,
				    AmfEncoding const amf_encoding)
{
    if (frontend && frontend->commandMessage) {
	MessageInfo msg_info;
	msg_info.msg_stream_id = chunk_stream->in_msg_stream_id;
	msg_info.timestamp = chunk_stream->in_msg_timestamp;

	Result res = Result::Failure;
	frontend.call_ret<Result> (&res, frontend->commandMessage, /*(*/
		&msg_info,
		&chunk_stream->page_list,
		chunk_stream->in_msg_len,
		amf_encoding /*)*/);
	return res;
    }

    return Result::Success;
}

Result
RtmpConnection::processUserControlMessage (ChunkStream * const chunk_stream)
{
    Byte const *msg_buf = chunk_stream->page_list.first->getData();
    Size const msg_len = chunk_stream->in_msg_len;

    Uint32 const uc_type = ((Uint32) msg_buf [0] << 8) |
			   ((Uint32) msg_buf [1] << 0);
    switch (uc_type) {
	case UserControlMessageType::StreamBegin: {
	    logD (msg, _func, "StreamBegin");
	  // No-op
	} break;
	case UserControlMessageType::StreamEof: {
	    logD (msg, _func, "StreamEof");
	  // No-op
	} break;
	case UserControlMessageType::StreamDry: {
	    logD (msg, _func, "StreamDry");
	  // No-op
	} break;
	case UserControlMessageType::SetBufferLength: {
	    logD (msg, _func, "SetBufferLength");
	  // No-op
	} break;
	case UserControlMessageType::StreamIsRecorded: {
	  // TODO Send "stream is recorded" to clients?
	    logD (msg, _func, "StreamIsRecorded");
	  // No-op
	} break;
	case UserControlMessageType::PingRequest: {
	    logD (msg, _func, "PingRequest");

	    if (msg_len < 6) {
		logE_ (_func, "PingRequest message is too short (", msg_len, " bytes)");
		return Result::Failure;
	    }

	    Uint32 const timestamp = ((Uint32) msg_buf [2] << 24) |
				     ((Uint32) msg_buf [3] << 16) |
				     ((Uint32) msg_buf [1] <<  8) |
				     ((Uint32) msg_buf [0] <<  0);
	    sendUserControl_PingResponse (timestamp);
	} break;
	case UserControlMessageType::PingResponse: {
	    logD_ (/* msg, */ _func, "PingResponse");

	    ping_reply_received = true;
	} break;
	default:
	    logW_ (_func, "unknown message type: ", uc_type);
    }

    return Result::Success;
}

void
RtmpConnection::senderClosed (Exception * const exc_,
			      void      * const _self)
{
    logD (close, _func, "0x", fmt_hex, (UintPtr) _self);

    RtmpConnection * const self = static_cast <RtmpConnection*> (_self);
    // TODO synchonization for is_closed
    self->is_closed = true;
    self->frontend.call (self->frontend->closed, exc_);
    self->backend.call (self->backend->close);
}

Receiver::ProcessInputResult
RtmpConnection::processInput (Memory const &mem,
			      Size * const mt_nonnull ret_accepted,
			      void * const _self)
{
    RtmpConnection * const self = static_cast <RtmpConnection*> (_self);
    return self->doProcessInput (mem, ret_accepted);
}

Receiver::ProcessInputResult
RtmpConnection::doProcessInput (ConstMemory const &mem,
				Size * const mt_nonnull ret_accepted)
{
    if (is_closed) {
	logD (close, _func, "0x", fmt_hex, (UintPtr) this, " is closed");
	return Receiver::ProcessInputResult::Error;
    }

    logD (msg, _func, "mem.len(): ", mem.len());
    if (logLevelOn (msg, LogLevel::Debug))
	hexdump (mem);

    *ret_accepted = 0;

    if (mem.len() == 0)
	return Receiver::ProcessInputResult::Again;

    processing_input = true;

    Byte const *data = mem.mem();
    Size len = mem.len();
    total_received += len;

    if (// Send acks only after something has been received actually (to avoid ack storms).
	remote_wack_size >= 2 &&
	// Sending acks twice as often as needed for extra safety.
	total_received - last_ack >= remote_wack_size / 2)
    {
	last_ack = total_received;
	sendAck (total_received /* seq */);
    }

    Receiver::ProcessInputResult ret_res = Receiver::ProcessInputResult::Normal;

    for (;;) {
	if (block_input) {
	    ret_res = Receiver::ProcessInputResult::InputBlocked;
	    goto _return;
	}

	switch (conn_state) {
	    case ReceiveState::Invalid:
		unreachable ();
	    case ReceiveState::ClientWaitS0: {
		if (len < 1) {
		    recv_needed_len = 1;
		    ret_res = Receiver::ProcessInputResult::Again;
		    goto _return;
		}
		recv_needed_len = 0;

		Byte const server_version = data [0];
		if (server_version < 3) {
		  // Deprecated protocols.
		    logE_ (_func, "ClientWaitS0: old protocol version: ", server_version);
		}

		{
		    data += 1;
		    len -= 1;
		}

		conn_state = ReceiveState::ClientWaitS1;
	    } break;
	    case ReceiveState::ClientWaitS1: {
		if (len < 1536) {
		    recv_needed_len = 1536;
		    ret_res = Receiver::ProcessInputResult::Again;
		    goto _return;
		}
		recv_needed_len = 0;

		{
		    PagePool::PageListHead page_list;
		    page_pool->getPages (&page_list, 1536 /* len */);
		    assert (page_list.first->data_len >= 1536);
		    Byte * const msg_c2 = page_list.first->getData();
		    memcpy (msg_c2, data, 4);
		    {
			Uint32 const time = getTimeMilliseconds ();
			msg_c2 [4] = (time >>  0) & 0xff;
			msg_c2 [5] = (time >>  8) & 0xff;
			msg_c2 [6] = (time >> 16) & 0xff;
			msg_c2 [7] = (time >> 24) & 0xff;
		    }
		    memcpy (msg_c2 + 8, data, 1536 - 8);
		    sendRawPages (page_list.first, 0 /* msg_offset */);
		}

		{
		    data += 1536;
		    len -= 1536;
		}

		conn_state = ReceiveState::ClientWaitS2;
	    } break;
	    case ReceiveState::ClientWaitS2: {
		if (len < 1536) {
		    recv_needed_len = 1536;
		    ret_res = Receiver::ProcessInputResult::Again;
		    goto _return;
		}
		recv_needed_len = 0;

		{
		    data += 1536;
		    len -= 1536;
		}

		if (frontend && frontend->handshakeComplete) {
		    Result res;
		    if (!frontend.call_ret<Result> (&res, frontend->handshakeComplete)) {
			ret_res = Receiver::ProcessInputResult::Error;
			goto _return;
		    }

		    if (!res) {
			ret_res = Receiver::ProcessInputResult::Error;
			goto _return;
		    }
		}

		beginPings ();

		conn_state = ReceiveState::BasicHeader;
	    } break;
	    case ReceiveState::ServerWaitC0: {
		logD (msg, _func, "ServerWaitC0");

		if (len < 1) {
		    recv_needed_len = 1;
		    ret_res = Receiver::ProcessInputResult::Again;
		    goto _return;
		}
		// TODO Get rid of recv_needed len if it is not very beneficial.
		recv_needed_len = 0;

		Byte const client_version = data [0];
		if (client_version < 3) {
		  // Deprecated protocols.
		    logE_ (_func, "ServerWaitC0: old protocol version: ", client_version);
		    ret_res = Receiver::ProcessInputResult::Error;
		    goto _return;
		}

		{
		    data += 1;
		    len -= 1;
		}

		{
		    PagePool::PageListHead page_list;
		    page_pool->getPages (&page_list, 1537 /* len */);
		    assert (page_list.first->data_len >= 1537);
		    Byte * const msg_s1 = page_list.first->getData();
		    msg_s1 [0] = 3;
		    {
			Uint32 const time = getTimeMilliseconds ();
			msg_s1 [1] = (time >>  0) & 0xff;
			msg_s1 [2] = (time >>  8) & 0xff;
			msg_s1 [3] = (time >> 16) & 0xff;
			msg_s1 [4] = (time >> 24) & 0xff;
		    }
		    memset (msg_s1 + 5, 0 , 4);
		    {
			unsigned n = 0;
			for (unsigned i = 9; i < 1537; ++i) {
			    n = (1536 + i + n) % 317;
			    msg_s1 [i] = n;
			}
		    }

		    sendRawPages (page_list.first, 0 /* msg_offset */);
		}

		conn_state = ReceiveState::ServerWaitC1;
	    } break;
	    case ReceiveState::ServerWaitC1: {
		logD (msg, _func, "ServerWaitC1");

		if (len < 1536) {
		    recv_needed_len = 1536;
		    ret_res = Receiver::ProcessInputResult::Again;
		    goto _return;
		}
		recv_needed_len = 0;

		{
		    PagePool::PageListHead page_list;
		    page_pool->getPages (&page_list, 1536 /* len */);
		    assert (page_list.first->data_len >= 1536);
		    Byte * const msg_s2 = page_list.first->getData();
		    memcpy (msg_s2, data, 4);
		    memset (msg_s2 + 4, 0, 4);
		    memcpy (msg_s2 + 8, data, 1536 - 8);
		    sendRawPages (page_list.first, 0 /* msg_offset */);
		}

		{
		    data += 1536;
		    len -= 1536;
		}

		conn_state = ReceiveState::ServerWaitC2;
	    } break;
	    case ReceiveState::ServerWaitC2: {
		logD (msg, _func, "ServerWaitC2");

		if (len < 1536) {
		    recv_needed_len = 1536;
		    ret_res = Receiver::ProcessInputResult::Again;
		    goto _return;
		}
		recv_needed_len = 0;

		{
		    data += 1536;
		    len -= 1536;
		}

		if (frontend && frontend->handshakeComplete) {
		    Result res;
		    if (!frontend.call_ret<Result> (&res, frontend->handshakeComplete)) {
			ret_res = Receiver::ProcessInputResult::Error;
			goto _return;
		    }

		    if (!res) {
			ret_res = Receiver::ProcessInputResult::Error;
			goto _return;
		    }
		}

		beginPings ();

		conn_state = ReceiveState::BasicHeader;
	    } break;
	    case ReceiveState::BasicHeader: {
		logD (msg, _func, "BasicHeader");

		if (len < 1) {
		    recv_needed_len = 1;
		    ret_res = Receiver::ProcessInputResult::Again;
		    logD (chunk, _func, "len < 1, returning Again");
		    goto _return;
		}
		recv_needed_len = 0;

		bool next_state = false;
		switch (cs_id__fmt) {
		    case CsIdFormat::Unknown: {
			fmt = (data [0] & 0xc0) >> 6;

			unsigned const local_cs_id = data [0] & 0x3f;
			switch (local_cs_id) {
			    case 0: {
			      // Ids 64-319
				cs_id = 64;
				cs_id__fmt = CsIdFormat::OneByte;
				logD (chunk, _func, "expecting CsIdFormat::OneByte");
			    } break;
			    case 1: {
			      // Ids 64-65536
				cs_id = 64;
				cs_id__fmt = CsIdFormat::TwoBytes_First;
				logD (chunk, _func, "expecting CsIdFormat::TwoBytes_First");
			    } break;
			    case 2: {
			      // Low-level protocol message
				cs_id = local_cs_id;
				next_state = true;
			    } break;
			    default: {
			      // Ids 3-63
				cs_id = local_cs_id;
				next_state = true;
			    }
			}
		    } break;
		    case CsIdFormat::OneByte: {
			cs_id += data [0];
			next_state = true;
		    } break;
		    case CsIdFormat::TwoBytes_First: {
			cs_id += data [0];
			cs_id__fmt = CsIdFormat::TwoBytes_Second;
			logD (chunk, _func, "expecting CsIdFormat::TwoBytes_Second");
		    } break;
		    case CsIdFormat::TwoBytes_Second: {
			cs_id += ((Uint16) data [0]) << 8;
			next_state = true;
		    } break;
		    default:
			unreachable ();
		}

		{
		    data += 1;
		    len -= 1;
		}

		if (next_state) {
		    logD (msg, _func, "chunk stream id: ", cs_id);

		    recv_chunk_stream = getChunkStream (cs_id, true /* create */);
		    logD (msg, _func, "recv_chunk_stream: 0x", fmt_hex, (UintPtr) recv_chunk_stream);
		    if (!recv_chunk_stream) {
			logE_ (_func, "stream not found: ", cs_id);
			ret_res = Receiver::ProcessInputResult::Error;
			goto _return;
		    }

		    switch (fmt) {
			case 0:
			    conn_state = ReceiveState::ChunkHeader_Type0;
			    break;
			case 1:
			    conn_state = ReceiveState::ChunkHeader_Type1;
			    break;
			case 2:
			    conn_state = ReceiveState::ChunkHeader_Type2;
			    break;
			case 3:
			    conn_state = ReceiveState::ChunkHeader_Type3;
			    break;
			default:
			    unreachable ();
		    }
		}
	    } break;
	    case ReceiveState::ChunkHeader_Type0: {
		logD (msg, _func, "ChunkHeader_Type0");

		if (len < 11) {
		    recv_needed_len = 11;
		    ret_res = Receiver::ProcessInputResult::Again;
		    goto _return;
		}
		recv_needed_len = 0;

		Uint32 const timestamp = (data [2] <<  0) |
					 (data [1] <<  8) |
					 (data [0] << 16);

		logD (time, _func, "rcv Type0 timestamp: 0x", fmt_hex, timestamp);

		bool has_extended_timestamp = false;
		if (timestamp == 0x00ffffff)
		    has_extended_timestamp = true;
		else
		    recv_chunk_stream->in_msg_timestamp = timestamp;

		recv_chunk_stream->in_msg_timestamp_delta = timestamp;
		recv_chunk_stream->in_msg_len = (data [5] <<  0) |
						(data [4] <<  8) |
						(data [3] << 16);
		recv_chunk_stream->in_msg_type_id = data [6];
		recv_chunk_stream->in_msg_stream_id = (data [ 7] <<  0) |
						      (data [ 8] <<  8) |
						      (data [ 9] << 16) |
						      (data [10] << 24);

		logD (msg, _func, "in_msg_len: ", recv_chunk_stream->in_msg_len, ", "
		      "in_msg_type_id: ", recv_chunk_stream->in_msg_type_id);

		logD (msg, _func, "in header is valid for chunk stream ", recv_chunk_stream->chunk_stream_id);
		recv_chunk_stream->in_header_valid = true;

		// TODO if (message is too long) ...

		{
		    data += 11;
		    len -= 11;
		}

		if (has_extended_timestamp) {
		    extended_timestamp_is_delta = false;
		    conn_state = ReceiveState::ExtendedTimestamp;
		} else
		    conn_state = ReceiveState::ChunkData;
	    } break;
	    case ReceiveState::ChunkHeader_Type1: {
		logD (msg, _func, "ChunkHeader_Type1");

		if (len < 7) {
		    recv_needed_len = 7;
		    ret_res = Receiver::ProcessInputResult::Again;
		    goto _return;
		}
		recv_needed_len = 0;

		if (!recv_chunk_stream->in_header_valid) {
		    logE_ (_func, "in_header is not valid, type 1, cs id ", recv_chunk_stream->chunk_stream_id);
		    ret_res = Receiver::ProcessInputResult::Error;
		    goto _return;
		}

		Uint32 const timestamp_delta = (data [2] <<  0) |
					       (data [1] <<  8) |
					       (data [0] << 16);

		logD (time, _func, "rcv Type1 timestamp_delta: 0x", fmt_hex, timestamp_delta);

		bool has_extended_timestamp = false;
		if (timestamp_delta == 0x00ffffff)
		    has_extended_timestamp = true;
		else
		    recv_chunk_stream->in_msg_timestamp += timestamp_delta;

		recv_chunk_stream->in_msg_timestamp_delta = timestamp_delta;
		recv_chunk_stream->in_msg_len = (data [5] <<  0) |
						(data [4] <<  8) |
						(data [3] << 16);
		recv_chunk_stream->in_msg_type_id = data [6];

		// TODO if (message is too long...)

		{
		    data += 7;
		    len -= 7;
		}

		if (has_extended_timestamp) {
		    extended_timestamp_is_delta = true;
		    conn_state = ReceiveState::ExtendedTimestamp;
		} else
		    conn_state = ReceiveState::ChunkData;
	    } break;
	    case ReceiveState::ChunkHeader_Type2: {
		logD (msg, _func, "ChunkHeader_Type2");

		if (len < 3) {
		    recv_needed_len = 3;
		    ret_res = Receiver::ProcessInputResult::Again;
		    goto _return;
		}
		recv_needed_len = 0;

		if (!recv_chunk_stream->in_header_valid) {
		    logE_ (_func, "in_header is not valid, type 2, cs id ", recv_chunk_stream->chunk_stream_id);
		    ret_res = Receiver::ProcessInputResult::Error;
		    goto _return;
		}

		Uint32 const timestamp_delta = (data [2] <<  0) |
					       (data [1] <<  8) |
					       (data [0] << 16);

		logD (time, _func, "rcv Type2 timestamp_delta: 0x", fmt_hex, timestamp_delta);

		bool has_extended_timestamp = false;
		if (timestamp_delta == 0x00ffffff)
		    has_extended_timestamp = true;
		else
		    recv_chunk_stream->in_msg_timestamp += timestamp_delta;

		recv_chunk_stream->in_msg_timestamp_delta = timestamp_delta;

		{
		    data += 3;
		    len -= 3;
		}

		if (has_extended_timestamp) {
		    extended_timestamp_is_delta = true;
		    conn_state = ReceiveState::ExtendedTimestamp;
		} else
		    conn_state = ReceiveState::ChunkData;
	    } break;
	    case ReceiveState::ChunkHeader_Type3: {
		logD (msg, _func, "ChunkHeader_Type3");

		if (len < 1) {
		    recv_needed_len = 1;
		    ret_res = Receiver::ProcessInputResult::Again;
		    goto _return;
		}
		recv_needed_len = 0;

		if (!recv_chunk_stream->in_header_valid) {
		    logE_ (_func, "in_header is not valid, type 3, cs id ", recv_chunk_stream->chunk_stream_id);
		    ret_res = Receiver::ProcessInputResult::Error;
		    goto _return;
		}

		bool has_extended_timestamp = false;
		if (recv_chunk_stream->in_msg_timestamp_delta >= 0x00ffffff)
		    has_extended_timestamp = true;

		if (recv_chunk_stream->in_msg_offset == 0)
		    recv_chunk_stream->in_msg_timestamp += recv_chunk_stream->in_msg_timestamp_delta;

		logD (msg, _func, "new msg timestamp: 0x", fmt_hex, recv_chunk_stream->in_msg_timestamp);

		if (has_extended_timestamp) {
		    // XXX false or true?
		    // This doesn't matter as long as we simply ignore
		    // the extended timestamp field and use the old value.
		    extended_timestamp_is_delta = false;
		    ignore_extended_timestamp = true;
		    conn_state = ReceiveState::ExtendedTimestamp;
		} else
		    conn_state = ReceiveState::ChunkData;
	    } break;
	    case ReceiveState::ExtendedTimestamp: {
		logD (msg, _func, "ExtendedTimestamp");

		if (len < 4) {
		    recv_needed_len = 4;
		    ret_res = Receiver::ProcessInputResult::Again;
		    goto _return;
		}
		recv_needed_len = 0;

		if (recv_chunk_stream->in_msg_offset == 0 &&
		    !ignore_extended_timestamp)
		{
		    Uint32 const extended_timestamp = (data [3] <<  0) |
						      (data [2] <<  8) |
						      (data [1] << 16) |
						      (data [0] << 24);
		    if (extended_timestamp_is_delta)
			recv_chunk_stream->in_msg_timestamp += extended_timestamp;
		    else
			recv_chunk_stream->in_msg_timestamp = extended_timestamp;
		}
		ignore_extended_timestamp = false;

		{
		    data += 4;
		    len -= 4;
		}

		conn_state = ReceiveState::ChunkData;
	    } break;
	    case ReceiveState::ChunkData: {
		logD (msg, _func, "ChunkData");

		if (!((recv_chunk_stream->in_msg_offset < recv_chunk_stream->in_msg_len) ||
		      (recv_chunk_stream->in_msg_len == 0 && recv_chunk_stream->in_msg_offset == 0)))
		{
		    logE_ (_func, "bad chunking: in_msg_offset: ", recv_chunk_stream->in_msg_offset, ", "
			   "in_msg_len: ", recv_chunk_stream->in_msg_len);
		    ret_res = Receiver::ProcessInputResult::Error;
		    goto _return;
		}

		logD (msg, _func, "in_msg_len: ", recv_chunk_stream->in_msg_len, ", in_msg_offset: ", recv_chunk_stream->in_msg_offset);
		Size const msg_left = recv_chunk_stream->in_msg_len - recv_chunk_stream->in_msg_offset;
		if (msg_left <= in_chunk_size) {
		  // Last chunk of a message.

		    logD (msg, _func, "last chunk");

#if 0
// Deprecated
		    if (len < recv_chunk_stream->in_msg_len - recv_chunk_stream->in_msg_offset) {
			// TODO There's no reason to wait: process partial chunks.
			// TODO recv_needed_len
			ret_res = Receiver::ProcessInputResult::Again;
			goto _return;
		    }
#endif

		    Size tofill = msg_left;
		    assert (chunk_offset < tofill);
		    tofill -= chunk_offset;
		    if (tofill > len)
			tofill = len;

		    if (prechunking_enabled &&
			    (recv_chunk_stream->in_msg_type_id == RtmpMessageType::AudioMessage ||
			     recv_chunk_stream->in_msg_type_id == RtmpMessageType::VideoMessage))
		    {
			Uint32 const out_chunk_stream_id =
				(recv_chunk_stream->in_msg_type_id == RtmpMessageType::AudioMessage ?
					 DefaultAudioChunkStreamId : DefaultVideoChunkStreamId);
			fillPrechunkedPages (&recv_chunk_stream->in_prechunk_ctx,
					     ConstMemory (data, tofill),
					     page_pool,
					     &recv_chunk_stream->page_list,
					     out_chunk_stream_id,
					     recv_chunk_stream->in_msg_timestamp,
					     recv_chunk_stream->in_msg_offset == 0 /* first_chunk */);
		    } else {
			page_pool->getFillPages (&recv_chunk_stream->page_list,
						 ConstMemory (data, tofill));
		    }

		    {
			data += tofill;
			len -= tofill;
		    }

		    chunk_offset += tofill;
		    assert (chunk_offset <= msg_left);
		    if (chunk_offset < msg_left) {
			ret_res = Receiver::ProcessInputResult::Again;
			goto _return;
		    }

		    Result const res = processMessage (recv_chunk_stream);

		    resetMessage (recv_chunk_stream);
		    resetPacket ();

		    if (!res) {
			logE_ (_func, "processMessage() failed");
			ret_res = Receiver::ProcessInputResult::Error;
			goto _return;
		    }
		} else {
		  // Intermediate chunk.

		    logD (msg, _func, "intermediate chunk");

#if 0
// Deprecated
		    if (len < in_chunk_size) {
			// TODO There's no reason to wait: process partial chunks.
			recv_needed_len = in_chunk_size;
			ret_res = Receiver::ProcessInputResult::Again;
			goto _return;
		    }
		    recv_needed_len = 0;
#endif

		    assert (chunk_offset < in_chunk_size);
		    Size tofill = in_chunk_size - chunk_offset;
		    if (tofill > len)
			tofill = len;

		    if (prechunking_enabled &&
			    (recv_chunk_stream->in_msg_type_id == RtmpMessageType::AudioMessage ||
			     recv_chunk_stream->in_msg_type_id == RtmpMessageType::VideoMessage))
		    {
			Uint32 const out_chunk_stream_id =
				(recv_chunk_stream->in_msg_type_id == RtmpMessageType::AudioMessage ?
					 DefaultAudioChunkStreamId : DefaultVideoChunkStreamId);
			fillPrechunkedPages (&recv_chunk_stream->in_prechunk_ctx,
					     ConstMemory (data, tofill),
					     page_pool,
					     &recv_chunk_stream->page_list,
					     out_chunk_stream_id,
					     recv_chunk_stream->in_msg_timestamp,
					     recv_chunk_stream->in_msg_offset == 0 /* first_chunk */);
		    } else {
			page_pool->getFillPages (&recv_chunk_stream->page_list,
						 ConstMemory (data, tofill));
		    }

		    {
			len -= tofill;
			data += tofill;
		    }

		    chunk_offset += tofill;
		    assert (chunk_offset <= in_chunk_size);
		    if (chunk_offset < in_chunk_size) {
			ret_res = Receiver::ProcessInputResult::Again;
			goto _return;
		    }

		    recv_chunk_stream->in_msg_offset += in_chunk_size;
		    resetPacket ();
		}
	    } break;
	    default:
		unreachable ();
	}
    } // for (;;)

_return:
    if (len != mem.len()) {
	if (len > mem.len()) {
	    logE_ (_func, "len > mem.len(): ", len, " > ", mem.len());
	    unreachable ();
	}
    }

    assert (len <= mem.len());
    *ret_accepted = mem.len() - len;

    processing_input = false;

    return ret_res;
}

void
RtmpConnection::processEof (void * const _self)
{
    logD (close, _func, "0x", fmt_hex, (UintPtr) _self);

    RtmpConnection * const self = static_cast <RtmpConnection*> (_self);

    self->is_closed = true;
    self->frontend.call (self->frontend->closed, /*(*/ (Exception*) NULL /* exc */);
    self->backend.call (self->backend->close);
}

void
RtmpConnection::processError (Exception * const exc_,
			      void      * const _self)
{
    logD (close, _func, "0x", fmt_hex, (UintPtr) _self);

    RtmpConnection * const self = static_cast <RtmpConnection*> (_self);

    self->is_closed = true;
    self->frontend.call (self->frontend->closed, /*(*/ exc_ /*)*/);
    self->backend.call (self->backend->close);
}

void
RtmpConnection::startClient ()
{
    logD_ (_func_);

    conn_state = ReceiveState::ClientWaitS0;

    PagePool::PageListHead page_list;
    page_pool->getPages (&page_list, 1537 /* len */);
    assert (page_list.first->data_len >= 1537);
    Byte * const msg_c1 = page_list.first->getData();
    msg_c1 [0] = 3;
    {
	Uint32 const time = getTimeMicroseconds ();
	msg_c1 [1] = (time >>  0) & 0xff;
	msg_c1 [2] = (time >>  8) & 0xff;
	msg_c1 [3] = (time >> 16) & 0xff;
	msg_c1 [4] = (time >> 24) & 0xff;
    }
    memset (msg_c1 + 5, 0, 4);
    {
	unsigned n = 0;
	for (unsigned i = 9; i < 1537; ++i) {
	    n = (1536 + i + n) % 317;
	    msg_c1 [i] = n;
	}
    }

    sendRawPages (page_list.first, 0 /* msg_offset */);
}

void
RtmpConnection::startServer ()
{
    conn_state = ReceiveState::ServerWaitC0;
}

Result
RtmpConnection::doConnect (MessageInfo * const mt_nonnull msg_info)
{
    sendWindowAckSize (local_wack_size);
    sendSetPeerBandwidth (remote_wack_size, 2 /* dynamic limitt */);
    sendUserControl_StreamBegin (0 /* msg_stream_id */);

    {
	AmfAtom atoms [18];
	AmfEncoder encoder (atoms);

	encoder.addString ("_result");
	encoder.addNumber (1.0);

	{
	    encoder.beginObject ();

	    encoder.addFieldName ("fmsVer");
	    encoder.addString ("MMNT/0,1,0,0");

	    encoder.addFieldName ("capabilities");
	    // TODO Define capabilities. Docs?
	    encoder.addNumber (31.0);

	    encoder.addFieldName ("mode");
	    encoder.addNumber (1.0);

	    encoder.endObject ();
	}

	{
	    encoder.beginObject ();

	    encoder.addFieldName ("level");
	    encoder.addString ("status");

	    encoder.addFieldName ("code");
	    encoder.addString ("NetConnection.Connect.Success");

	    encoder.addFieldName ("description");
	    encoder.addString ("Connection succeded");

	    encoder.endObject ();
	}

	Byte msg_buf [512];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "encode() failed");
	    return Result::Failure;
	}

	sendCommandMessage_AMF0 (msg_info->msg_stream_id, ConstMemory (msg_buf, msg_len));
	logD (send, _func, "msg_len: ", msg_len);
	if (logLevelOn (send, LogLevel::Debug))
	    hexdump (ConstMemory (msg_buf, msg_len));
    }

    return Result::Success;
}

Result
RtmpConnection::doCreateStream (MessageInfo * mt_nonnull msg_info,
				AmfDecoder  * mt_nonnull amf_decoder)
{
    double transaction_id;
    if (!amf_decoder->decodeNumber (&transaction_id)) {
	logE_ (_func, "could not decode transaction_id");
	return Result::Failure;
    }

    // TODO Perhaps a unique message stream id should be allocated.
    //      (a simple increment of a counter would do).
    double const msg_stream_id = DefaultMessageStreamId;

    {
	AmfAtom atoms [4];
	AmfEncoder encoder (atoms);

	encoder.addString ("_result");
	encoder.addNumber (transaction_id);
	encoder.addNullObject ();
	encoder.addNumber (msg_stream_id);

	Byte msg_buf [512];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "encode() failed");
	    return Result::Failure;
	}

	sendCommandMessage_AMF0 (msg_info->msg_stream_id, ConstMemory (msg_buf, msg_len));
    }

    return Result::Success;
}

// Deprecated constructor
RtmpConnection::RtmpConnection (Object     * const coderef_container,
				Timers     * mt_nonnull const timers,
				PagePool   * mt_nonnull const page_pool)
    : DependentCodeReferenced (coderef_container),

      timers (timers),
	
      page_pool (page_pool),
      sender (NULL),

      prechunking_enabled (true),

      is_closed (false),

      ping_send_timer (NULL),
      ping_reply_received (false),

      in_chunk_size  (DefaultChunkSize),
      out_chunk_size (DefaultChunkSize),

      extended_timestamp_is_delta (false),
      ignore_extended_timestamp (false),

      processing_input (false),
      block_input (false),

      remote_wack_size (1 << 20 /* 1 Mb */),

      recv_needed_len (0),
      total_received (0),
      last_ack (0),

      conn_state (ReceiveState::Invalid),

      local_wack_size (1 << 20 /* 1 Mb */)
{
    resetPacket ();

    control_chunk_stream = getChunkStream (2, true /* create */);
    data_chunk_stream    = getChunkStream (3, true /* create */);
}

void
RtmpConnection::init (Timers     * mt_nonnull const timers,
		      PagePool   * mt_nonnull const page_pool)
{
    this->timers = timers;
    this->page_pool = page_pool;
}

RtmpConnection::RtmpConnection (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container),

      timers (NULL),
	
      page_pool (NULL),
      sender (NULL),

      prechunking_enabled (true),

      is_closed (false),

      ping_send_timer (NULL),
      ping_reply_received (false),

      in_chunk_size  (DefaultChunkSize),
      out_chunk_size (DefaultChunkSize),

      extended_timestamp_is_delta (false),
      ignore_extended_timestamp (false),

      processing_input (false),
      block_input (false),

      remote_wack_size (1 << 20 /* 1 Mb */),

      recv_needed_len (0),
      total_received (0),
      last_ack (0),

      conn_state (ReceiveState::Invalid),

      local_wack_size (1 << 20 /* 1 Mb */)
{
    resetPacket ();

    control_chunk_stream = getChunkStream (2, true /* create */);
    data_chunk_stream    = getChunkStream (3, true /* create */);
}

RtmpConnection::~RtmpConnection ()
{
    if (ping_send_timer)
	timers->deleteTimer (ping_send_timer);

    // TODO Release chunk streams
    // TODO Release incomplete messages in chunk streams.
}

}

