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


#ifndef __LIBMOMENT__VIDEO_STREAM__H__
#define __LIBMOMENT__VIDEO_STREAM__H__


#include <libmary/libmary.h>

#include <moment/amf_decoder.h>


namespace Moment {

using namespace M;

class RtmpConnection;

class VideoStream : public Object
{
public:
    class AudioFrameType
    {
    public:
	enum Value {
	    Unknown = 0,
	    RawData,
	    AacSequenceHeader
	};
	operator Value () const { return value; }
	AudioFrameType (Value const value) : value (value) {}
	AudioFrameType () {}
	Size toString_ (Memory const &mem, Format const &fmt);
    private:
	Value value;
    };

    class VideoFrameType
    {
    public:
	enum Value {
	    Unknown = 0,
	    KeyFrame,             // for AVC, a seekable frame
	    InterFrame,           // for AVC, a non-seekable frame
	    DisposableInterFrame, // H.264 only
	    GeneratedKeyFrame,    // reserved for server use (according to FLV format spec.)
	    CommandFrame,         // video info / command frame,
	    AvcSequenceHeader,
	    AvcEndOfSequence,
	  // The following message types should be sent to clients as RTMP
	  // command messages.
	    RtmpSetMetaData,
	    RtmpClearMetaData
	};
	operator Value () const { return value; }
	VideoFrameType (Value const value) : value (value) {}
	VideoFrameType () {}
	Size toString_ (Memory const &mem, Format const &fmt);
    private:
	Value value;

    public:
	static VideoFrameType fromFlvFrameType (Byte flv_frame_type);

	Byte toFlvFrameType () const;
    };

    class AudioCodecId
    {
    public:
	enum Value {
	    Unknown = 0,
	    LinearPcmPlatformEndian,
	    ADPCM,
	    MP3,
	    LinearPcmLittleEndian,
	    Nellymoser_16kHz_mono,
	    Nellymoser_8kHz_mono,
	    Nellymoser,
	    G711ALaw,      // reserved
	    G711MuLaw,     // reserved
	    AAC,
	    Speex,
	    MP3_8kHz,      // reserved
	    DeviceSpecific // reserved
	};
	operator Value () const { return value; }
	AudioCodecId (Value const value) : value (value) {}
	AudioCodecId () {}
	Size toString_ (Memory const &mem, Format const &fmt);
    private:
	Value value;

    public:
	static AudioCodecId fromFlvCodecId (Byte flv_codec_id);

	Byte toFlvCodecId () const;
    };

    class VideoCodecId
    {
    public:
	enum Value {
	    Unknown = 0,
	    SorensonH263,  // Sorenson H.263
	    ScreenVideo,   // Screen video
	    ScreenVideoV2, // Screen video version 2
	    VP6,           // On2 VP6
	    VP6Alpha,      // On2 VP6 with alpha channel
	    AVC            // h.264 / AVC
	};
	operator Value () const { return value; }
	VideoCodecId (Value const value) : value (value) {}
	VideoCodecId () {}
	Size toString_ (Memory const &mem, Format const &fmt);
    private:
	Value value;

    public:
	static VideoCodecId fromFlvCodecId (Byte flv_codec_id);

	Byte toFlvCodecId () const;
    };

    // Must be copyable.
    class MessageInfo
    {
    public:
	Uint64 timestamp;

	// Greater than zero for prechunked messages.
	Uint32 prechunk_size;

	MessageInfo ()
	    : timestamp (0),
	      prechunk_size (0)
	{
	}
    };

    // Must be copyable.
    struct AudioMessageInfo : public MessageInfo
    {
    public:
	AudioFrameType frame_type;
	AudioCodecId codec_id;

	AudioMessageInfo ()
	    : frame_type (AudioFrameType::Unknown),
	       codec_id (AudioCodecId::Unknown)
	{
	}
    };

    // Must be copyable.
    struct VideoMessageInfo : public MessageInfo
    {
    public:
      // Note that we ignore AVC composition time for now.

	enum MessageType {
	    VideoFrame,
	    Metadata
	};

	VideoFrameType frame_type;
	VideoCodecId codec_id;

	// TODO Get rid of is_keyframe in favor of frame_type.
	bool is_keyframe;

	VideoMessageInfo ()
	    : frame_type (VideoFrameType::Unknown),
	      codec_id (VideoCodecId::Unknown),
	      is_keyframe (true)
	{
	}
    };

    struct EventHandler
    {
	void (*audioMessage) (AudioMessageInfo       * mt_nonnull msg_info,
			      PagePool               * mt_nonnull page_pool,
			      PagePool::PageListHead * mt_nonnull page_list,
			      Size                    msg_len,
			      Size                    msg_offset,
			      void                  *cb_data);

	void (*videoMessage) (VideoMessageInfo       * mt_nonnull msg_info,
			      PagePool               * mt_nonnull page_pool,
			      PagePool::PageListHead * mt_nonnull page_list,
			      Size                    msg_len,
			      Size                    msg_offset,
			      void                   *cb_data);

	void (*rtmpCommandMessage) (RtmpConnection    * mt_nonnull conn,
				    MessageInfo       * mt_nonnull msg_info,
				    ConstMemory const &method_name,
				    AmfDecoder        * mt_nonnull amf_decoder,
				    void              *cb_data);

	void (*closed) (void *cb_data);
    };

    // TODO Rename to SavedVideoFrame.
    struct SavedFrame
    {
	VideoStream::VideoMessageInfo msg_info;
	PagePool *page_pool;
	PagePool::PageListHead page_list;
	Size msg_len;
	Size msg_offset;
    };

    struct SavedAudioFrame
    {
	VideoStream::AudioMessageInfo msg_info;
	PagePool *page_pool;
	PagePool::PageListHead page_list;
	Size msg_len;
	Size msg_offset;
    };

    mt_unsafe class FrameSaver
    {
    private:
	bool got_saved_keyframe;
	SavedFrame saved_keyframe;

	bool got_saved_metadata;
	SavedFrame saved_metadata;

	bool got_saved_aac_seq_hdr;
	SavedAudioFrame saved_aac_seq_hdr;

	bool got_saved_avc_seq_hdr;
	SavedFrame saved_avc_seq_hdr;

    public:
	void processAudioFrame (AudioMessageInfo       * mt_nonnull msg_info,
				PagePool               * mt_nonnull page_pool,
				PagePool::PageListHead * mt_nonnull page_list,
				Size                    msg_len,
				Size                    msg_offset);

	void processVideoFrame (VideoMessageInfo       * mt_nonnull msg_info,
				PagePool               * mt_nonnull page_pool,
				PagePool::PageListHead * mt_nonnull page_list,
				Size                    msg_len,
				Size                    msg_offset);

	bool getSavedKeyframe (SavedFrame * mt_nonnull ret_frame);

	bool getSavedMetaData (SavedFrame * mt_nonnull ret_frame);

	bool getSavedAacSeqHdr (SavedAudioFrame * mt_nonnull ret_frame);

	bool getSavedAvcSeqHdr (SavedFrame * mt_nonnull ret_frame);

	FrameSaver ();

	~FrameSaver ();
    };

private:
    mt_mutex (mutex) FrameSaver frame_saver;

    Informer_<EventHandler> event_informer;

    static void informAudioMessage (EventHandler *event_handler,
				    void *cb_data,
				    void *inform_data);

    static void informVideoMessage (EventHandler *event_handler,
				    void *cb_data,
				    void *inform_data);

    static void informRtmpCommandMessage (EventHandler *event_handler,
					  void *cb_data,
					  void *inform_data);

    static void informClosed (EventHandler *event_handler,
			      void *cb_data,
			      void *inform_data);

public:
    // Returned FrameSaver must be synchronized manually with VideoStream::lock/unlock().
    FrameSaver* getFrameSaver ()
    {
	return &frame_saver;
    }

    // It is guaranteed that the informer can be controlled with
    // VideoStream::lock/unlock() methods.
    Informer_<EventHandler>* getEventInformer ()
    {
	return &event_informer;
    }

    void fireAudioMessage (AudioMessageInfo       * mt_nonnull msg_info,
			   PagePool               * mt_nonnull page_pool,
			   PagePool::PageListHead * mt_nonnull page_list,
			   Size                    msg_len,
			   Size                    msg_offset);

    void fireVideoMessage (VideoMessageInfo       * mt_nonnull msg_info,
			   PagePool               * mt_nonnull page_pool,
			   PagePool::PageListHead * mt_nonnull page_list,
			   Size                    msg_len,
			   Size                    msg_offset);

    void fireRtmpCommandMessage (RtmpConnection    * mt_nonnull conn,
				 MessageInfo       * mt_nonnull msg_info,
				 ConstMemory const &method_name,
				 AmfDecoder        * mt_nonnull amf_decoder);

    void close ();

    void lock ()
    {
	mutex.lock ();
    }

    void unlock ()
    {
	mutex.unlock ();
    }

    VideoStream ();

    ~VideoStream ();
};

}


#include <moment/rtmp_connection.h>


#endif /* __LIBMOMENT__VIDEO_STREAM__H__ */

