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
	    AacSequenceHeader,
	    SpeexHeader
	};
	operator Value () const { return value; }
	AudioFrameType (Value const value) : value (value) {}
	AudioFrameType () {}
	Size toString_ (Memory const &mem, Format const &fmt);
    private:
	Value value;

    public:
	bool isAudioData () const
	{
	    return value == RawData;
	}
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

	bool isVideoData () const
	{
	    return value == KeyFrame             ||
		   value == InterFrame           ||
		   value == DisposableInterFrame ||
		   value == GeneratedKeyFrame;
	}

	bool isKeyFrame () const
	{
	    return isVideoData() && (value == KeyFrame || value == GeneratedKeyFrame);
	}

	bool isInterFrame () const
	{
	    return isVideoData() && (value == InterFrame || value == DisposableInterFrame);
	}

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
    class Message
    {
    public:
	Uint64 timestamp;

        PagePool *page_pool;
        PagePool::PageListHead page_list;
        Size msg_len;
        Size msg_offset;

	// Greater than zero for prechunked messages.
	Uint32 prechunk_size;

	Message ()
	    : timestamp (0),

	      page_pool (NULL),
	      msg_len (0),
	      msg_offset (0),

	      prechunk_size (0)
	{
	}
    };

    // Must be copyable.
    struct AudioMessage : public Message
    {
    public:
	AudioFrameType frame_type;
	AudioCodecId codec_id;

	AudioMessage ()
	    : frame_type (AudioFrameType::Unknown),
	       codec_id (AudioCodecId::Unknown)
	{
	}
    };

    // Must be copyable.
    struct VideoMessage : public Message
    {
    public:
      // Note that we ignore AVC composition time for now.

	enum MessageType {
	    VideoFrame,
	    Metadata
	};

	VideoFrameType frame_type;
	VideoCodecId codec_id;

	VideoMessage ()
	    : frame_type (VideoFrameType::Unknown),
	      codec_id (VideoCodecId::Unknown)
	{
	}
    };

    struct EventHandler
    {
	void (*audioMessage) (AudioMessage * mt_nonnull msg,
			      void         *cb_data);

	void (*videoMessage) (VideoMessage * mt_nonnull msg,
			      void         *cb_data);

	void (*rtmpCommandMessage) (RtmpConnection    * mt_nonnull conn,
				    Message           * mt_nonnull msg,
				    ConstMemory const &method_name,
				    AmfDecoder        * mt_nonnull amf_decoder,
				    void              *cb_data);

	// FIXME getVideoStream() and closed() imply a race condition.
	// Add isClosed() method to VideoStream as a workaround.
	void (*closed) (void *cb_data);
    };

    // TODO Rename to SavedVideoFrame.
    struct SavedFrame
    {
	VideoStream::VideoMessage msg;
    };

    struct SavedAudioFrame
    {
	VideoStream::AudioMessage msg;
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

	List<SavedAudioFrame*> saved_speex_headers;

	void releaseSavedSpeexHeaders ();

    public:
	void processAudioFrame (AudioMessage * mt_nonnull msg);

	void processVideoFrame (VideoMessage * mt_nonnull msg);

	bool getSavedKeyframe (SavedFrame *ret_frame);

	bool getSavedMetaData (SavedFrame * mt_nonnull ret_frame);

	bool getSavedAacSeqHdr (SavedAudioFrame * mt_nonnull ret_frame);

	bool getSavedAvcSeqHdr (SavedFrame * mt_nonnull ret_frame);

	Size getNumSavedSpeexHeaders ();

	void getSavedSpeexHeaders (SavedAudioFrame *ret_frames,
				   Size             ret_frame_size);

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

    void fireAudioMessage (AudioMessage * mt_nonnull msg);

    void fireVideoMessage (VideoMessage * mt_nonnull msg);

    void fireRtmpCommandMessage (RtmpConnection    * mt_nonnull conn,
				 Message           * mt_nonnull msg,
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

