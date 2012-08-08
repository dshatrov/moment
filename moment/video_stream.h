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
#if 0
    private:
        // TODO Remove the "must be copyable" comments
        Message& operator = (Message const &);
        Message (Message const &);
#endif

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
        bool is_saved_frame;

	VideoMessage ()
	    : frame_type (VideoFrameType::Unknown),
	      codec_id (VideoCodecId::Unknown),
              is_saved_frame (false)
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

        void (*numWatchersChanged) (Count  num_watchers,
                                    void  *cb_data);
    };

    // TODO Rename to SavedVideoFrame.
    // TODO Get rid of Saved*Frame in favor of VideoStream::Audio/VideoMessage
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

        List<SavedFrame> saved_interframes;

	bool got_saved_metadata;
	SavedFrame saved_metadata;

	bool got_saved_aac_seq_hdr;
	SavedAudioFrame saved_aac_seq_hdr;

	bool got_saved_avc_seq_hdr;
	SavedFrame saved_avc_seq_hdr;

	List<SavedAudioFrame*> saved_speex_headers;

        void releaseSavedInterframes ();

	void releaseSavedSpeexHeaders ();

        void releaseState ();

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

        void copyStateFrom (FrameSaver *frame_saver);

        struct FrameHandler
        {
            Result (*audioFrame) (AudioMessage * mt_nonnull audio_msg,
                                  void         *cb_data);

            Result (*videoFrame) (VideoMessage * mt_nonnull video_msg,
                                  void         *cb_data);
        };

        Result reportSavedFrames (FrameHandler const * mt_nonnull frame_handler,
                                  void               *cb_data);

	FrameSaver ();

	~FrameSaver ();
    };

private:
    class PendingFrameList_name;

    class PendingFrame : public IntrusiveListElement<PendingFrameList_name>
    {
    public:
        enum Type {
            t_Audio,
            t_Video
        };

    private:
        Type const type;

    public:
        Type getType() const
        {
            return type;
        }

        PendingFrame (Type const type)
            : type (type)
        {
        }
    };

    typedef IntrusiveList<PendingFrame, PendingFrameList_name> PendingFrameList;

    class PendingAudioFrame : public PendingFrame
    {
    public:
        AudioMessage audio_msg;

        PendingAudioFrame ()
            : PendingFrame (t_Audio)
        {
        }
    };

    class PendingVideoFrame : public PendingFrame
    {
    public:
        VideoMessage video_msg;

        PendingVideoFrame ()
            : PendingFrame (t_Video)
        {
        }
    };

    mt_mutex (mutex) bool is_closed;

    mt_mutex (mutex) FrameSaver frame_saver;

    mt_mutex (mutex) Count num_watchers;

  // ___________________________ Stream binding data ___________________________

    class BindTicket : public Referenced
    {
    public:
        VideoStream *video_stream;
        VideoStream *bind_stream;
    };

    mt_mutex (mutex) Count bind_inform_counter;

    mt_mutex (mutex) Uint64 timestamp_offs;
    mt_mutex (mutex) Uint64 last_adjusted_timestamp;
    mt_mutex (mutex) bool   got_timestamp_offs;

    mt_mutex (mutex) Uint64 pending_timestamp_offs;
    mt_mutex (mutex) bool   pending_got_timestamp_offs;

    mt_mutex (mutex) Ref<BindTicket> cur_bind_ticket;
    mt_mutex (mutex) Ref<BindTicket> pending_bind_ticket;

    mt_mutex (mutex) FrameSaver pending_frame_saver;
    mt_mutex (mutex) PendingFrameList pending_frame_list;

    mt_mutex (mutex) WeakRef<VideoStream> weak_bind_stream;
    mt_mutex (mutex) GenericInformer::SubscriptionKey bind_sbn;

    // Protects from concurrent invocations of bindToSrteam()
    Mutex bind_mutex;

  mt_iface (FrameSaver::FrameHandler)

    static FrameSaver::FrameHandler const bind_frame_handler;

    static mt_unlocks_locks (mutex) Result bind_savedAudioFrame (AudioMessage * mt_nonnull audio_msg,
                                                                 void         *_self);

    static mt_unlocks_locks (mutex) Result bind_savedVideoFrame (VideoMessage * mt_nonnull video_msg,
                                                                 void         *_self);

  mt_iface_end

    bool bind_messageBegin (Message    * mt_nonnull msg,
                            BindTicket *bind_ticket,
                            bool        is_audio_msg,
                            Uint64     * mt_nonnull ret_timestamp_offs);

    mt_unlocks_locks (mutex) void bind_messageEnd ();

  // ___________________________________________________________________________

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

private:
    static void informNumWatchersChanged (EventHandler *event_handler,
                                          void         *cb_data,
                                          void         *inform_data);

    mt_unlocks_locks (mutex) void fireNumWatchersChanged (Count num_watchers);

    static void watcherDeletionCallback (void *_self);

public:
    // 'guard_obj' must be unique. Only one pluOneWatcher() call should be made
    // for any 'guard_obj' instance. If 'guard_obj' is not null, then minusOneWatcher()
    // should not be called to cancel the effect of plusOneWatcher().
    // That will be done automatically when 'guard_obj' is destroyed.
    mt_async void plusOneWatcher (Object *guard_obj = NULL);
    mt_async mt_unlocks_locks (mutex) void plusOneWatcher_unlocked (Object *guard_obj = NULL);

    mt_async void minusOneWatcher ();
    mt_async mt_unlocks_locks (mutex) void minusOneWatcher_unlocked ();

    mt_async void plusWatchers (Count delta);
    mt_async mt_unlocks_locks (mutex) void plusWatchers_unlocked (Count delta);

    mt_async void minusWatchers (Count delta);
    mt_async mt_unlocks_locks (mutex) void minusWatchers_unlocked (Count delta);

    mt_mutex (mutex) bool getNumWatchers_unlocked ()
    {
        return num_watchers;
    }

private:
  mt_iface (VideoStream::EventHandler)

    static EventHandler const bind_handler;

    static void bind_audioMessage (AudioMessage * mt_nonnull msg,
                                   void         *_self);

    static void bind_videoMessage (VideoMessage * mt_nonnull msg,
                                   void         *_self);

    static void bind_rtmpCommandMessage (RtmpConnection    * mt_nonnull conn,
                                         Message           * mt_nonnull msg,
                                         ConstMemory const &method_name,
                                         AmfDecoder        * mt_nonnull amf_decoder,
                                         void              *_self);

  mt_iface_end

public:
    void bindToStream (VideoStream *bind_stream);

    void close ();

    mt_mutex (mutex) bool isClosed_unlocked ()
    {
        return is_closed;
    }

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

