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
	Result (*connect) (ConstMemory const &app_name,
			   void *cb_data);

	Result (*startStreaming) (ConstMemory const &stream_name,
				  void  *cb_data);

	Result (*startWatching) (ConstMemory const &stream_name,
				 void  *cb_data);

	CommandResult (*commandMessage) (RtmpConnection              * mt_nonnull conn,
					 ConstMemory const           &method_name,
					 RtmpConnection::MessageInfo * mt_nonnull msg_info,
					 AmfDecoder                  * mt_nonnull amf_decoder,
					 void                        *cb_data);
    };

private:
    mt_const RtmpConnection *rtmp_conn;
    mt_const RtmpConnection::ChunkStream *audio_chunk_stream;
    mt_const RtmpConnection::ChunkStream *video_chunk_stream;

    Cb<Frontend> frontend;

    AtomicInt playing;

    Result doConnect (RtmpConnection::MessageInfo * mt_nonnull msg_info,
		      AmfDecoder * const mt_nonnull decoder);

    Result doPlay (RtmpConnection::MessageInfo * mt_nonnull msg_info,
		   AmfDecoder * mt_nonnull decoder);

    Result doPublish (RtmpConnection::MessageInfo * mt_nonnull msg_info,
		      AmfDecoder * mt_nonnull decoder);

public:
  // sendVideoMessage() and sendAudioMessage() are here and not in
  // RtmpConnection because of audio_chunk_stream/video_chunk_stream.

    void sendVideoMessage (VideoStream::VideoMessageInfo * mt_nonnull msg_info,
			   PagePool::PageListHead        * mt_nonnull page_list,
			   Size                           msg_len,
			   Size                           msg_offset);

    void sendAudioMessage (VideoStream::AudioMessageInfo * mt_nonnull msg_info,
			   PagePool::PageListHead        * mt_nonnull page_list,
			   Size                           msg_len,
			   Size                           msg_offset);

    // Helper method. Sends saved onMetaData, AvcSequenceHeader, KeyFrame.
    void sendInitialMessages_unlocked (VideoStream::FrameSaver * mt_nonnull frame_saver);

    Result commandMessage (RtmpConnection::MessageInfo * mt_nonnull msg_info,
			   PagePool                    * mt_nonnull page_pool,
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

    class MetaData
    {
    public:
      // GStreamer's FLV muxer:

	// General:
	//   duration
	//   filesize
	//   creator
	//   title

	// Video:
	//   videocodecid
	//   width
	//   height
	//   AspectRatioX
	//   AspectRatioY
	//   framerate

	// Audio:
	//   audiocodecid

	// creationdate

      // Ffmpeg FLV muxer:

	// General:
	//   duration
	//   filesize

	// Video:
	//   width
	//   height
	//   videodatarate
	//   framerate
	//   videocodecid

	// Audio:
	//   audiodatarate
	//   audiosamplerate
	//   audiosamplesize
	//   stereo
	//   audiocodecid

	enum GotFlags {
	    VideoCodecId    = 0x0001,
	    AudioCodecId    = 0x0002,

	    VideoWidth      = 0x0004,
	    VideoHeight     = 0x0008,

	    AspectRatioX    = 0x0010,
	    AspectRatioY    = 0x0020,

	    VideoDataRate   = 0x0040,
	    AudioDataRate   = 0x0080,

	    VideoFrameRate  = 0x0100,

	    AudioSampleRate = 0x0200,
	    AudioSampleSize = 0x0400,

	    NumChannels     = 0x0800
	};

	VideoStream::VideoCodecId video_codec_id;
	VideoStream::AudioCodecId audio_codec_id;

	Uint32 video_width;
	Uint32 video_height;

	Uint32 aspect_ratio_x;
	Uint32 aspect_ratio_y;

	Uint32 video_data_rate;
	Uint32 audio_data_rate;

	Uint32 video_frame_rate;

	Uint32 audio_sample_rate;
	Uint32 audio_sample_size;

	Uint32 num_channels;

	Uint32 got_flags;

	MetaData ()
	    : got_flags (0)
	{
	}
    };

    static Result encodeMetaData (MetaData * mt_nonnull metadata,
				  PagePool * mt_nonnull page_pool,
				  PagePool::PageListHead * mt_nonnull page_list,
				  Size     *ret_msg_len,
				  VideoStream::VideoMessageInfo *ret_msg_info);

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

