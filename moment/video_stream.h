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


#ifndef __LIBMOMENT__VIDEO_STREAM__H__
#define __LIBMOMENT__VIDEO_STREAM__H__


#include <libmary/libmary.h>

#include <moment/amf_decoder.h>


namespace Moment {

using namespace M;

class RtmpConnection;

class StreamParameters : public Referenced
{
private:
    StringHash< Ref<String> > params;

public:
    bool hasParam (ConstMemory const name)
    {
        StringHash< Ref<String> >::EntryKey const key = params.lookup (name);
        return (bool) key;
    }

    ConstMemory getParam (ConstMemory   const name,
                          bool        * const ret_has_param = NULL)
    {
        StringHash< Ref<String> >::EntryKey const key = params.lookup (name);
        if (!key) {
            if (ret_has_param)
                *ret_has_param = false;

            return ConstMemory ();
        }

        if (ret_has_param)
            *ret_has_param = true;

        return key.getDataPtr()->ptr()->mem();
    }

    void setParam (ConstMemory const name,
                   ConstMemory const value)
    {
        params.add (name, grab (new String (value)));
    }
};

class VideoStream : public Object
{
private:
    // Protects from concurrent invocations of bindToSrteam()
    Mutex bind_mutex;

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

        bool hasTimestamp ()
        {
            return isVideoData() ||
                   value == AvcSequenceHeader ||
                   value == AvcEndOfSequence;
        }

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
        Uint64 timestamp_nanosec;

        PagePool *page_pool;
        PagePool::PageListHead page_list;
        Size msg_len;
        Size msg_offset;

	// Greater than zero for prechunked messages.
	Uint32 prechunk_size;

	Message ()
	    : timestamp_nanosec (0),

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
        unsigned rate;
        unsigned channels;

	AudioMessage ()
            : frame_type (AudioFrameType::Unknown),
              codec_id   (AudioCodecId::Unknown),
              rate       (44100),
              channels   (1)
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
        // TODO 'is_saved_frame' seems unused and unnecessary now.
        bool is_saved_frame;

	VideoMessage ()
	    : frame_type (VideoFrameType::Unknown),
	      codec_id (VideoCodecId::Unknown),
              is_saved_frame (false)
	{
	}
    };

    static bool msgHasTimestamp (Message * const msg,
                                 bool      const is_audio_msg)
    {
        bool has_timestamp = true;
        if (!is_audio_msg) {
            VideoMessage * const video_msg = static_cast <VideoMessage*> (msg);
            has_timestamp = video_msg->frame_type.hasTimestamp ();
        }

        return has_timestamp;
    }

    struct EventHandler
    {
	void (*audioMessage) (AudioMessage * mt_nonnull audio_msg,
			      void         *cb_data);

	void (*videoMessage) (VideoMessage * mt_nonnull video_msg,
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

    // TODO Move FrameSaver to frame_saver.h,
    //      Move VideoCodecId and friends to av_message.h
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

    public:
        void releaseState (bool release_audio = true,
                           bool release_video = true);

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

    mt_const Ref<StreamParameters> stream_params;

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

// TODO make private
public:
    class BindInfo
    {
    public:
        Uint64 timestamp_offs;
        bool   got_timestamp_offs;

        Uint64 pending_timestamp_offs;
        bool   pending_got_timestamp_offs;

        Ref<BindTicket> cur_bind_ticket;
        Ref<BindTicket> pending_bind_ticket;

        FrameSaver pending_frame_saver;
        PendingFrameList pending_frame_list;

        WeakRef<VideoStream> weak_bind_stream;
        GenericInformer::SubscriptionKey bind_sbn;

        void reset ()
        {
            timestamp_offs = 0;
            got_timestamp_offs = false;

            pending_timestamp_offs = 0;
            pending_got_timestamp_offs = false;

            cur_bind_ticket = NULL;
            pending_bind_ticket = NULL;

            pending_frame_saver.releaseState ();

            {
                PendingFrameList::iter iter (pending_frame_list);
                while (!pending_frame_list.iter_done (iter)) {
                    PendingFrame * const pending_frame = pending_frame_list.iter_next (iter);
                    delete pending_frame;
                }
                pending_frame_list.clear ();
            }

            weak_bind_stream = NULL;
            bind_sbn = NULL;
        }

        BindInfo ()
        {
            reset ();
        }

        ~BindInfo ()
        {
            reset ();
        }
    };

private:
    // TODO 1. got_stream_timestamp. 'stream_timestamp' may be unknown.
    //      2. set initial stream timestamp to some shifted value to compensate for possible slight timestamp drift,
    //         like +10 minutes.
    mt_mutex (mutex) Uint64 stream_timestamp_nanosec;

    mt_mutex (mutex) BindInfo abind;
    mt_mutex (mutex) BindInfo vbind;

#if 0
    // if 'true', then both audio and video are bound to the same stream,
    // and the binding is described by 'vbind' ('abind' is clear).
    bool full_bind;
#endif

    mt_mutex (mutex) Count bind_inform_counter;

  mt_iface (FrameSaver::FrameHandler)

    static FrameSaver::FrameHandler const bind_frame_handler;

    static mt_unlocks_locks (mutex) Result bind_savedAudioFrame (AudioMessage * mt_nonnull audio_msg,
                                                                 void         *_self);

    static mt_unlocks_locks (mutex) Result bind_savedVideoFrame (VideoMessage * mt_nonnull video_msg,
                                                                 void         *_self);

  mt_iface_end

    mt_mutex (mutex) bool bind_messageBegin (BindInfo   * mt_nonnull bind_info,
                                             Message    * mt_nonnull msg,
                                             BindTicket *bind_ticket,
                                             bool        is_audio_msg);

    mt_unlocks_locks (mutex) void bind_doMessageEnd (BindInfo * mt_nonnull bind_info);

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

    static EventHandler const abind_handler;
    static EventHandler const vbind_handler;

    static void bind_audioMessage (AudioMessage * mt_nonnull msg,
                                   void         *_bind_ticket);

    static void bind_videoMessage (VideoMessage * mt_nonnull msg,
                                   void         *_bind_ticket);

    static void bind_rtmpCommandMessage (RtmpConnection    * mt_nonnull conn,
                                         Message           * mt_nonnull msg,
                                         ConstMemory const &method_name,
                                         AmfDecoder        * mt_nonnull amf_decoder,
                                         void              *_bind_ticket);

  mt_iface_end

public:
    void bindToStream (VideoStream *bind_audio_stream,
                       VideoStream *bind_video_stream,
                       bool         bind_audio,
                       bool         bind_video);

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

    bool hasParam (ConstMemory const name)
    {
        if (!stream_params)
            return false;

        return stream_params->hasParam (name);
    }

    ConstMemory getParam (ConstMemory   const name,
                          bool        * const ret_has_param = NULL)
    {
        if (!stream_params) {
            if (ret_has_param)
                *ret_has_param = false;

            return ConstMemory ();
        }

        return stream_params->getParam (name, ret_has_param);
    }

    mt_const void setStreamParameters (StreamParameters * const stream_params)
    {
        assert (!this->stream_params);
        this->stream_params = stream_params;
    }

    VideoStream ();

    ~VideoStream ();
};

}


#include <moment/rtmp_connection.h>


#endif /* __LIBMOMENT__VIDEO_STREAM__H__ */

