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


#ifndef __LIBMOMENT__RTMP_SERVER__H__
#define __LIBMOMENT__RTMP_SERVER__H__


#include <libmary/libmary.h>

#include <moment/rtmp_connection.h>
#include <moment/video_stream.h>


namespace Moment {

using namespace M;

// TODO BasicRtmpServer
class RtmpServer
{
public:
    class CommandResult
    {
    public:
	enum Value {
	    Failure = 0,
	    Success,
	    UnknownCommand
	};
	operator Value () const { return value; }
	CommandResult (Value const value) : value (value) {}
	CommandResult () {}
    private:
	Value value;
    };

    struct Frontend {
	// StartStreaming
	Result (*startStreaming) (ConstMemory const &stream_name,
				  void  *cb_data);

	Result (*startWatching) (ConstMemory const &stream_name,
				 void  *cb_data);

	CommandResult (*commandMessage) (RtmpConnection *conn,
					 ConstMemory const &method_name,
					 RtmpConnection::MessageInfo *msg_info,
					 AmfDecoder *amf_decoder,
					 void *cb_data);
    };

private:
    mt_const RtmpConnection *rtmp_conn;
    mt_const RtmpConnection::ChunkStream *audio_chunk_stream;
    mt_const RtmpConnection::ChunkStream *video_chunk_stream;

    Cb<Frontend> frontend;

    AtomicInt playing;

    Result doPlay (RtmpConnection::MessageInfo * mt_nonnull msg_info,
		   AmfDecoder * mt_nonnull decoder);

    Result doPublish (RtmpConnection::MessageInfo * mt_nonnull msg_info,
		      AmfDecoder * mt_nonnull decoder);

public:
  // sendVideoMessage() and sendAudioMessage() are here and not in
  // RtmpConnection because of audio_chunk_stream/video_chunk_stream.

    void sendVideoMessage (RtmpConnection::MessageInfo * mt_nonnull msg_info,
			   PagePool::PageListHead      * mt_nonnull page_list,
			   Size                         msg_len);

    void sendAudioMessage (RtmpConnection::MessageInfo * mt_nonnull msg_info,
			   PagePool::PageListHead      * mt_nonnull page_list,
			   Size                         msg_len);

    Result commandMessage (RtmpConnection::MessageInfo * mt_nonnull msg_info,
			   PagePool::PageListHead      * mt_nonnull page_list,
			   Size                         msg_len,
			   AmfEncoding                  amf_encoding);

    void setFrontend (Cb<Frontend> const &frontend)
    {
	this->frontend = frontend;
    }

    // Must be called only once for initialization.
    void setRtmpConnection (RtmpConnection * const rtmp_conn)
    {
	this->rtmp_conn = rtmp_conn;
	audio_chunk_stream = rtmp_conn->getChunkStream (RtmpConnection::DefaultAudioChunkStreamId, true /* create */);
	video_chunk_stream = rtmp_conn->getChunkStream (RtmpConnection::DefaultVideoChunkStreamId, true /* create */);
    }

    // TODO Deprecated constructor, delete.
    RtmpServer (RtmpConnection * const rtmp_conn)
	: rtmp_conn (rtmp_conn),
	  playing (0)
    {
	audio_chunk_stream = rtmp_conn->getChunkStream (RtmpConnection::DefaultAudioChunkStreamId, true /* create */);
	video_chunk_stream = rtmp_conn->getChunkStream (RtmpConnection::DefaultVideoChunkStreamId, true /* create */);
    }

    RtmpServer ()
	: rtmp_conn (NULL),           // extra initializer
	  audio_chunk_stream (NULL),  // extra initializer
	  video_chunk_stream (NULL),  // extra initializer
	  playing (0)
    {
    }
};

}


#endif /* __LIBMOMENT__RTMP_SERVER__H__ */

