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


#include <moment/video_stream.h>


using namespace M;

static LogGroup libMary_logGroup_frames ("VideoStream.frames", LogLevel::I);
static LogGroup libMary_logGroup_frame_saver ("frame_saver", LogLevel::I);

namespace Moment {

Size
VideoStream::AudioFrameType::toString_ (Memory const &mem,
					Format const & /* fmt */) const
{
    switch (value) {
	case Unknown:
	    return toString (mem, "Unknown");
	case RawData:
	    return toString (mem, "RawData");
	case AacSequenceHeader:
	    return toString (mem, "AacSequenceHeader");
	case SpeexHeader:
	    return toString (mem, "SpeexHeader");
    }

    unreachable ();
    return 0;
}

Size
VideoStream::VideoFrameType::toString_ (Memory const &mem,
					Format const & /* fmt */) const
{
    switch (value) {
	case Unknown:
	    return toString (mem, "Unknown");
	case KeyFrame:
	    return toString (mem, "KeyFrame");
	case InterFrame:
	    return toString (mem, "InterFrame");
	case DisposableInterFrame:
	    return toString (mem, "DisposableInterFrame");
	case GeneratedKeyFrame:
	    return toString (mem, "GeneratedKeyFrame");
	case CommandFrame:
	    return toString (mem, "CommandFrame");
	case AvcSequenceHeader:
	    return toString (mem, "AvcSequenceHeader");
	case AvcEndOfSequence:
	    return toString (mem, "AvcEndOfSequence");
	case RtmpSetMetaData:
	    return toString (mem, "RtmpSetMetaData");
	case RtmpClearMetaData:
	    return toString (mem, "RtmpClearMetaData");
    }

    unreachable ();
    return 0;
}

Size
VideoStream::AudioCodecId::toString_ (Memory const &mem,
				      Format const & /* fmt */) const
{
    switch (value) {
	case Unknown:
	    return toString (mem, "Unknown");
	case LinearPcmPlatformEndian:
	    return toString (mem, "LinearPcmPlatformEndian");
	case ADPCM:
	    return toString (mem, "ADPCM");
	case MP3:
	    return toString (mem, "MP3");
	case LinearPcmLittleEndian:
	    return toString (mem, "LinearPcmLittleEndian");
	case Nellymoser_16kHz_mono:
	    return toString (mem, "Nellymoser_16kHz_mono");
	case Nellymoser_8kHz_mono:
	    return toString (mem, "Nellymoser_8kHz_mono");
	case Nellymoser:
	    return toString (mem, "Nellymoser");
	case G711ALaw:
	    return toString (mem, "G711ALaw");
	case G711MuLaw:
	    return toString (mem, "G711MuLaw");
	case AAC:
	    return toString (mem, "AAC");
	case Speex:
	    return toString (mem, "Speex");
	case MP3_8kHz:
	    return toString (mem, "MP3_8kHz");
	case DeviceSpecific:
	    return toString (mem, "DeviceSpecific");
    }

    unreachable ();
    return 0;
}

Size
VideoStream::VideoCodecId::toString_ (Memory const &mem,
				      Format const & /* fmt */) const
{
    switch (value) {
	case Unknown:
	    return toString (mem, "Unknown");
	case SorensonH263:
	    return toString (mem, "SorensonH263");
	case ScreenVideo:
	    return toString (mem, "ScreenVideo");
	case ScreenVideoV2:
	    return toString (mem, "ScreenVideoV2");
	case VP6:
	    return toString (mem, "VP6");
	case VP6Alpha:
	    return toString (mem, "VP6Alpha");
	case AVC:
	    return toString (mem, "AVC");
    }

    unreachable ();
    return 0;
}

VideoStream::VideoFrameType
VideoStream::VideoFrameType::fromFlvFrameType (Byte const flv_frame_type)
{
    switch (flv_frame_type) {
	case 1:
	    return KeyFrame;
	case 2:
	    return InterFrame;
	case 3:
	    return DisposableInterFrame;
	case 4:
	    return GeneratedKeyFrame;
	case 5:
	    return CommandFrame;
    }

    return Unknown;
}

Byte
VideoStream::VideoFrameType::toFlvFrameType () const
{
    switch (value) {
	case Unknown:
	    return 0;
	case AvcSequenceHeader:
	case AvcEndOfSequence:
	case KeyFrame:
	    return 1;
	case InterFrame:
	    return 2;
	case DisposableInterFrame:
	    return 3;
	case GeneratedKeyFrame:
	    return 4;
	case CommandFrame:
	    return 5;
	case RtmpSetMetaData:
	case RtmpClearMetaData:
	    unreachable ();
    }

    unreachable ();
    return 0;
}

VideoStream::AudioCodecId
VideoStream::AudioCodecId::fromFlvCodecId (Byte const flv_codec_id)
{
    switch (flv_codec_id) {
	case 0:
	    return LinearPcmPlatformEndian;
	case 1:
	    return ADPCM;
	case 2:
	    return MP3;
	case 3:
	    return LinearPcmLittleEndian;
	case 4:
	    return Nellymoser_16kHz_mono;
	case 5:
	    return Nellymoser_8kHz_mono;
	case 6:
	    return Nellymoser;
	case 7:
	    return G711ALaw;
	case 8:
	    return G711MuLaw;
	case 10:
	    return AAC;
	case 11:
	    return Speex;
	case 14:
	    return MP3_8kHz;
	case 15:
	    return DeviceSpecific;
    }

    return Unknown;
}

Byte
VideoStream::AudioCodecId::toFlvCodecId () const
{
    switch (value) {
	case Unknown:
	    return (Byte) -1;
	case LinearPcmPlatformEndian:
	    return 0;
	case ADPCM:
	    return 1;
	case MP3:
	    return 2;
	case LinearPcmLittleEndian:
	    return 3;
	case Nellymoser_16kHz_mono:
	    return 4;
	case Nellymoser_8kHz_mono:
	    return 5;
	case Nellymoser:
	    return 6;
	case G711ALaw:
	    return 7;
	case G711MuLaw:
	    return 8;
	case AAC:
	    return 10;
	case Speex:
	    return 11;
	case MP3_8kHz:
	    return 14;
	case DeviceSpecific:
	    return 15;
    }

    unreachable ();
    return (Byte) -1;
}

VideoStream::VideoCodecId
VideoStream::VideoCodecId::fromFlvCodecId (Byte const flv_codec_id)
{
    switch (flv_codec_id) {
	case 2:
	    return SorensonH263;
	case 3:
	    return ScreenVideo;
	case 4:
	    return VP6;
	case 5:
	    return VP6Alpha;
	case 6:
	    return ScreenVideoV2;
	case 7:
	    return AVC;
    }

    return Unknown;
}

Byte
VideoStream::VideoCodecId::toFlvCodecId () const
{
    switch (value) {
	case Unknown:
	    return 0;
	case SorensonH263:
	    return 2;
	case ScreenVideo:
	    return 3;
	case VP6:
	    return 4;
	case VP6Alpha:
	    return 5;
	case ScreenVideoV2:
	    return 6;
	case AVC:
	    return 7;
    }

    unreachable ();
    return 0;
}

void
VideoStream::FrameSaver::processAudioFrame (AudioMessage * const mt_nonnull msg)
{
    logD (frame_saver, _func, "0x", fmt_hex, (UintPtr) this);

#if 0
    if (msg.page_list->first && msg.page_list->first->data_len >= 1)
	logD_ (_func, "audio header: 0x", fmt_hex, (unsigned) msg.page_list->first->getData() [0]);
#endif

    switch (msg->frame_type) {
	case AudioFrameType::AacSequenceHeader: {
	    logD (frame_saver, _func, msg->frame_type);

	    logD (frames, _func, "AAC SEQUENCE HEADER");

            logS_ (_this_func, "AacSequenceHeader");

	    if (got_saved_aac_seq_hdr)
		saved_aac_seq_hdr.msg.page_pool->msgUnref (saved_aac_seq_hdr.msg.page_list.first);

	    got_saved_aac_seq_hdr = true;
	    saved_aac_seq_hdr.msg = *msg;

	    msg->page_pool->msgRef (msg->page_list.first);
	} break;
	case AudioFrameType::SpeexHeader: {
	    logD (frames, _func, "SPEEX HEADER");

	    if (saved_speex_headers.getNumElements() >= 2) {
		logD (frames, _func, "Wrapping saved speex headers");
		releaseSavedSpeexHeaders ();
	    }

	    SavedAudioFrame * const frame = new SavedAudioFrame;
	    assert (frame);
	    frame->msg = *msg;
	    msg->page_pool->msgRef (msg->page_list.first);

	    saved_speex_headers.append (frame);
	} break;
	default:
	  // No-op
	    ;
    }
}

void
VideoStream::FrameSaver::processVideoFrame (VideoMessage * const mt_nonnull msg)
{
    logD (frame_saver, _func, "0x", fmt_hex, (UintPtr) this, " ", msg->frame_type);

    switch (msg->frame_type) {
	case VideoFrameType::KeyFrame:
        case VideoFrameType::GeneratedKeyFrame: {
	    if (got_saved_keyframe)
		saved_keyframe.msg.page_pool->msgUnref (saved_keyframe.msg.page_list.first);

            releaseSavedInterframes ();

	    got_saved_keyframe = true;
	    saved_keyframe.msg = *msg;
            saved_keyframe.msg.is_saved_frame = true;

	    msg->page_pool->msgRef (msg->page_list.first);
	} break;
        case VideoFrameType::InterFrame:
        case VideoFrameType::DisposableInterFrame: {
            if (!got_saved_keyframe)
                return;

            if (saved_interframes.getNumElements() >= 1000 /* TODO Config parameter for saved frames window. */) {
                logD_ (_func, "Too many interframes to save");
                return;
            }

            saved_interframes.appendEmpty ();
            SavedFrame * const new_frame = &saved_interframes.getLast();
            new_frame->msg = *msg;

	    msg->page_pool->msgRef (msg->page_list.first);
        } break;
	case VideoFrameType::AvcSequenceHeader: {
	    if (got_saved_avc_seq_hdr)
		saved_avc_seq_hdr.msg.page_pool->msgUnref (saved_avc_seq_hdr.msg.page_list.first);

	    got_saved_avc_seq_hdr = true;
	    saved_avc_seq_hdr.msg = *msg;
            saved_avc_seq_hdr.msg.is_saved_frame = true;

	    msg->page_pool->msgRef (msg->page_list.first);
	} break;
	case VideoFrameType::AvcEndOfSequence: {
	    if (got_saved_avc_seq_hdr)
		saved_avc_seq_hdr.msg.page_pool->msgUnref (saved_avc_seq_hdr.msg.page_list.first);

	    got_saved_avc_seq_hdr = false;
	} break;
	case VideoFrameType::RtmpSetMetaData: {
	    if (got_saved_metadata)
		saved_metadata.msg.page_pool->msgUnref (saved_metadata.msg.page_list.first);

	    got_saved_metadata = true;
	    saved_metadata.msg = *msg;
            saved_metadata.msg.is_saved_frame = true;

	    msg->page_pool->msgRef (msg->page_list.first);
	} break;
	case VideoFrameType::RtmpClearMetaData: {
	    if (got_saved_metadata)
		saved_metadata.msg.page_pool->msgUnref (saved_metadata.msg.page_list.first);

	    got_saved_metadata = false;
	} break;
	default:
	  // No-op
	    ;
    }
}

void
VideoStream::FrameSaver::copyStateFrom (FrameSaver * const frame_saver)
{
    logD_ (_this_func_);

    releaseState ();

    got_saved_keyframe = frame_saver->got_saved_keyframe;
    saved_keyframe = frame_saver->saved_keyframe;
    saved_keyframe.msg.page_pool->msgRef (saved_keyframe.msg.page_list.first);

    got_saved_metadata = frame_saver->got_saved_metadata;
    saved_metadata = frame_saver->saved_metadata;
    saved_metadata.msg.page_pool->msgRef (saved_metadata.msg.page_list.first);

    got_saved_aac_seq_hdr = frame_saver->got_saved_aac_seq_hdr;
    saved_aac_seq_hdr = frame_saver->saved_aac_seq_hdr;
    saved_aac_seq_hdr.msg.page_pool->msgRef (saved_aac_seq_hdr.msg.page_list.first);

    got_saved_avc_seq_hdr = frame_saver->got_saved_avc_seq_hdr;
    saved_avc_seq_hdr = frame_saver->saved_avc_seq_hdr;
    saved_avc_seq_hdr.msg.page_pool->msgRef (saved_avc_seq_hdr.msg.page_list.first);

    {
        saved_interframes.clear ();
        List<SavedFrame>::iter iter (frame_saver->saved_interframes);
        while (!frame_saver->saved_interframes.iter_done (iter)) {
            SavedFrame * const frame = &frame_saver->saved_interframes.iter_next (iter)->data;
            saved_interframes.appendEmpty();
            SavedFrame * const new_frame = &saved_interframes.getLast();
            *new_frame = *frame;
            new_frame->msg.page_pool->msgRef (new_frame->msg.page_list.first);
        }
    }

    {
        saved_speex_headers.clear ();
        List<SavedAudioFrame*>::iter iter (frame_saver->saved_speex_headers);
        while (!frame_saver->saved_speex_headers.iter_done (iter)) {
            SavedAudioFrame * const frame = frame_saver->saved_speex_headers.iter_next (iter)->data;
            SavedAudioFrame * const new_frame = new SavedAudioFrame (*frame);
            assert (new_frame);
            new_frame->msg.page_pool->msgRef (new_frame->msg.page_list.first);
            saved_speex_headers.append (new_frame);
        }
    }
}

Result
VideoStream::FrameSaver::reportSavedFrames (FrameHandler const * const mt_nonnull frame_handler,
                                            void               * const cb_data)
{
    if (got_saved_metadata) {
        if (frame_handler->videoFrame) {
            if (!frame_handler->videoFrame (&saved_metadata.msg, cb_data))
                return Result::Failure;
        }
    }

    if (got_saved_aac_seq_hdr) {
        if (frame_handler->audioFrame) {
            if (!frame_handler->audioFrame (&saved_aac_seq_hdr.msg, cb_data))
                return Result::Failure;
        }
    }

    // TODO AAC end of sequence - ?
    if (got_saved_avc_seq_hdr
        && frame_handler->videoFrame)
    {
        {
            VideoStream::VideoMessage msg;

            msg.timestamp_nanosec = saved_avc_seq_hdr.msg.timestamp_nanosec;
            msg.codec_id = VideoStream::VideoCodecId::AVC;
            msg.frame_type = VideoStream::VideoFrameType::AvcEndOfSequence;

            msg.page_pool = saved_avc_seq_hdr.msg.page_pool;
            msg.prechunk_size = 0;
            msg.msg_offset = 0;

            msg.is_saved_frame = true;

          // TODO Send AvcEndOfSequence only when AvcSequenceHeader was sent.

            if (!frame_handler->videoFrame (&msg, cb_data))
                return Result::Failure;
        }

        if (!frame_handler->videoFrame (&saved_avc_seq_hdr.msg, cb_data))
            return Result::Failure;
    }

    if (frame_handler->audioFrame) {
        List<SavedAudioFrame*>::iter iter (saved_speex_headers);
        while (!saved_speex_headers.iter_done (iter)) {
            SavedAudioFrame * const frame = saved_speex_headers.iter_next (iter)->data;
            if (!frame_handler->audioFrame (&frame->msg, cb_data))
                return Result::Failure;
        }
    }

    if (got_saved_keyframe) {
        if (frame_handler->videoFrame)
            if (!frame_handler->videoFrame (&saved_keyframe.msg, cb_data))
                return Result::Failure;
    }

    {
        List<SavedFrame>::iter iter (saved_interframes);
        while (!saved_interframes.iter_done (iter)) {
            SavedFrame * const frame = &saved_interframes.iter_next (iter)->data;
            if (frame_handler->videoFrame) {
               if (!frame_handler->videoFrame (&frame->msg, cb_data))
                   return Result::Failure;
            }
        }
    }

    return Result::Success;
}

void
VideoStream::FrameSaver::releaseSavedInterframes ()
{
    List<SavedFrame>::iter iter (saved_interframes);
    while (!saved_interframes.iter_done (iter)) {
        SavedFrame * const frame = &saved_interframes.iter_next (iter)->data;
	frame->msg.page_pool->msgUnref (frame->msg.page_list.first);
    }

    saved_interframes.clear ();
}

void
VideoStream::FrameSaver::releaseSavedSpeexHeaders ()
{
    List<SavedAudioFrame*>::iter iter (saved_speex_headers);
    while (!saved_speex_headers.iter_done (iter)) {
	SavedAudioFrame * const frame = saved_speex_headers.iter_next (iter)->data;
	frame->msg.page_pool->msgUnref (frame->msg.page_list.first);
	delete frame;
    }

    saved_speex_headers.clear ();
}

void
VideoStream::FrameSaver::releaseState (bool const release_audio,
                                       bool const release_video)
{
    logD_ (_this_func_);

    if (release_video) {
        if (got_saved_keyframe) {
            saved_keyframe.msg.page_pool->msgUnref (saved_keyframe.msg.page_list.first);
            got_saved_keyframe = false;
        }

        if (got_saved_avc_seq_hdr) {
            saved_avc_seq_hdr.msg.page_pool->msgUnref (saved_avc_seq_hdr.msg.page_list.first);
            got_saved_avc_seq_hdr = false;
        }

        releaseSavedInterframes ();
    }

    if (release_audio) {
        if (got_saved_aac_seq_hdr) {
            saved_aac_seq_hdr.msg.page_pool->msgUnref (saved_aac_seq_hdr.msg.page_list.first);
            got_saved_aac_seq_hdr = false;
        }

        releaseSavedSpeexHeaders ();
    }

    // TODO When and how to release this for bound streams?
    if (got_saved_metadata) {
	saved_metadata.msg.page_pool->msgUnref (saved_metadata.msg.page_list.first);
        got_saved_metadata = false;
    }
}

VideoStream::FrameSaver::FrameSaver ()
    : got_saved_keyframe (false),
      got_saved_metadata (false),
      got_saved_aac_seq_hdr (false),
      got_saved_avc_seq_hdr (false)
{
}

VideoStream::FrameSaver::~FrameSaver ()
{
    releaseState ();
}

namespace {
    struct InformAudioMessage_Data {
	VideoStream::AudioMessage *msg;

	InformAudioMessage_Data (VideoStream::AudioMessage * const msg)
	    : msg (msg)
	{
	}
    };
}

void
VideoStream::informAudioMessage (EventHandler * const event_handler,
				 void * const cb_data,
				 void * const _inform_data)
{
    if (event_handler->audioMessage) {
        InformAudioMessage_Data * const inform_data =
                static_cast <InformAudioMessage_Data*> (_inform_data);
	event_handler->audioMessage (inform_data->msg, cb_data);
    }
}

namespace {
    struct InformVideoMessage_Data {
	VideoStream::VideoMessage *msg;

	InformVideoMessage_Data (VideoStream::VideoMessage * const msg)
	    : msg (msg)
	{
	}
    };
}

void
VideoStream::informVideoMessage (EventHandler * const event_handler,
				 void * const cb_data,
				 void * const _inform_data)
{
    if (event_handler->videoMessage) {
        InformVideoMessage_Data * const inform_data =
                static_cast <InformVideoMessage_Data*> (_inform_data);
	event_handler->videoMessage (inform_data->msg, cb_data);
    }
}

namespace {
    struct InformRtmpCommandMessage_Data {
	RtmpConnection           *conn;
	VideoStream::Message     *msg;
	ConstMemory        const &method_name;
	AmfDecoder               *amf_decoder;

	InformRtmpCommandMessage_Data (RtmpConnection       * const  conn,
				       VideoStream::Message * const  msg,
				       ConstMemory            const &method_name,
				       AmfDecoder           * const  amf_decoder)
	    : conn (conn),
	      msg (msg),
	      method_name (method_name),
	      amf_decoder (amf_decoder)
	{
	}
    };
}

void
VideoStream::informRtmpCommandMessage (EventHandler * const event_handler,
				       void * const cb_data,
				       void * const _inform_data)
{
    // TODO Save/restore amf_decoder state between callback invocations.
    //      Viable option - abstract away the parsing process.
    if (event_handler->rtmpCommandMessage) {
        InformRtmpCommandMessage_Data * const inform_data =
                static_cast <InformRtmpCommandMessage_Data*> (_inform_data);
	event_handler->rtmpCommandMessage (inform_data->conn,
					   inform_data->msg,
					   inform_data->method_name,
					   inform_data->amf_decoder,
					   cb_data);
    }
}

void
VideoStream::informClosed (EventHandler * const event_handler,
			   void * const cb_data,
			   void * const /* inform_data */)
{
    if (event_handler->closed)
	event_handler->closed (cb_data);
}

void
VideoStream::fireAudioMessage (AudioMessage * const mt_nonnull msg)
{
    logS_ (_this_func, "ts ", msg->timestamp_nanosec, " ", msg->frame_type);

    mutex.lock ();

    InformAudioMessage_Data inform_data (msg);
    mt_unlocks_locks (mutex) event_informer.informAll_unlocked (informAudioMessage, &inform_data);

    frame_saver.processAudioFrame (msg);

    mutex.unlock ();
}

void
VideoStream::fireVideoMessage (VideoMessage * const mt_nonnull msg)
{
    logS_ (_this_func, "ts ", msg->timestamp_nanosec, " ", msg->frame_type);

    mutex.lock ();

    InformVideoMessage_Data inform_data (msg);
    mt_unlocks_locks (mutex) event_informer.informAll_unlocked (informVideoMessage, &inform_data);

    frame_saver.processVideoFrame (msg);

    mutex.unlock ();
}

void
VideoStream::fireRtmpCommandMessage (RtmpConnection * const  mt_nonnull conn,
				     Message        * const  mt_nonnull msg,
				     ConstMemory      const &method_name,
				     AmfDecoder     * const  mt_nonnull amf_decoder)
{
    InformRtmpCommandMessage_Data inform_data (conn, msg, method_name, amf_decoder);
    event_informer.informAll (informRtmpCommandMessage, &inform_data);
}

namespace {
    struct InformNumWatchersChanged_Data {
        Count num_watchers;

        InformNumWatchersChanged_Data (Count const num_watchers)
            : num_watchers (num_watchers)
	{
	}
    };
}

void
VideoStream::informNumWatchersChanged (EventHandler *event_handler,
                                       void         *cb_data,
                                       void         *_inform_data)
{
    if (event_handler->numWatchersChanged) {
        InformNumWatchersChanged_Data * const inform_data =
                static_cast <InformNumWatchersChanged_Data*> (_inform_data);
        event_handler->numWatchersChanged (inform_data->num_watchers, cb_data);
    }
}

mt_unlocks_locks (mutex) void
VideoStream::fireNumWatchersChanged (Count const num_watchers)
{
    InformNumWatchersChanged_Data inform_data (num_watchers);
    event_informer.informAll_unlocked (informNumWatchersChanged, &inform_data);
}

void
VideoStream::watcherDeletionCallback (void * const _self)
{
    VideoStream * const self = static_cast <VideoStream*> (_self);
    self->minusOneWatcher ();
}

mt_async void
VideoStream::plusOneWatcher (Object * const guard_obj)
{
    mutex.lock ();
    mt_async mt_unlocks_locks (mutex) plusOneWatcher_unlocked (guard_obj);
    mutex.unlock ();
}

mt_async mt_unlocks_locks (mutex) void
VideoStream::plusOneWatcher_unlocked (Object * const guard_obj)
{
    // Keep in mind that bindToStream() can be called at any moment.
    // Calling bind_stream->plusOneWatcher() *before* changing 'num_watchers'
    // to avoid races.
    for (;;) {
        Ref<VideoStream> const abind_stream = abind.weak_bind_stream.getRef();
        Ref<VideoStream> const vbind_stream = vbind.weak_bind_stream.getRef();

        if (!abind_stream && !vbind_stream)
            break;

        mutex.unlock ();

        if (abind_stream)
            abind_stream->plusOneWatcher ();

        if (vbind_stream)
            vbind_stream->plusOneWatcher ();

        mutex.lock ();

        logD_ (_func, "--- abind: 0x", fmt_hex, (UintPtr) abind.weak_bind_stream.getTypedWeakPtr(),
               ", abind_stream: 0x", (UintPtr) abind_stream.ptr());

        if (abind.weak_bind_stream.getTypedWeakPtr() == abind_stream &&
            vbind.weak_bind_stream.getTypedWeakPtr() == vbind_stream)
        {
            break;
        }

        mutex.unlock ();

        if (abind_stream)
            abind_stream->minusOneWatcher ();

        if (vbind_stream)
            vbind_stream->minusOneWatcher ();

        mutex.lock ();
    }

    ++num_watchers;

    logD_ (_func, "calling fireNumWatchersChanged()");
    mt_async mt_unlocks_locks (mutex) fireNumWatchersChanged (num_watchers);

    if (guard_obj) {
        guard_obj->addDeletionCallback (
                CbDesc<Object::DeletionCallback> (watcherDeletionCallback,
                                                  this /* cb_data */,
                                                  this /* guard_obj */));
    }
}

mt_async void
VideoStream::minusOneWatcher ()
{
    mutex.lock ();
    mt_async minusOneWatcher_unlocked ();
    mutex.unlock ();
}

mt_async mt_unlocks_locks (mutex) void
VideoStream::minusOneWatcher_unlocked ()
{
    assert (num_watchers > 0);
    --num_watchers;

    // Keep in mind that bindToStream() can be called at any moment.
    // Calling bind_stream->minusOneWatcher() *after* changing 'num_watchers'
    // to avoid races.
    Ref<VideoStream> const abind_stream = abind.weak_bind_stream.getRef ();
    Ref<VideoStream> const vbind_stream = vbind.weak_bind_stream.getRef ();
    if (abind_stream ||
        vbind_stream)
    {
        mutex.unlock ();

        if (abind_stream)
            mt_async abind_stream->minusOneWatcher ();

        if (vbind_stream)
            mt_async vbind_stream->minusOneWatcher ();

        mutex.lock ();
    }

    logD_ (_func, "calling fireNumWatchersChanged()");
    mt_async mt_unlocks_locks (mutex) fireNumWatchersChanged (num_watchers);
}

mt_async void
VideoStream::plusWatchers (Count const delta)
{
    if (delta == 0)
        return;

    mutex.lock ();
    mt_async mt_unlocks_locks (mutex) plusWatchers_unlocked (delta);
    mutex.unlock ();
}

mt_async mt_unlocks_locks (mutex) void
VideoStream::plusWatchers_unlocked (Count const delta)
{
    if (delta == 0)
        return;

    // Keep in mind that bindToStream() can be called at any moment.
    // Calling bind_stream->plusOneWatcher() *before* changing 'num_watchers'
    // to avoid races.
    for (;;) {
        Ref<VideoStream> const abind_stream = abind.weak_bind_stream.getRef();
        Ref<VideoStream> const vbind_stream = vbind.weak_bind_stream.getRef();

        if (!abind_stream && !vbind_stream)
            break;

        mutex.unlock ();

        if (abind_stream)
            abind_stream->plusWatchers (delta);

        if (vbind_stream)
            vbind_stream->plusWatchers (delta);

        mutex.lock ();

        if (abind.weak_bind_stream.getTypedWeakPtr() == abind_stream &&
            vbind.weak_bind_stream.getTypedWeakPtr() == vbind_stream)
        {
          // Ok, moving on.
            break;
        }

        logD_ (_func, "extra iteration");

        mutex.unlock ();

        if (abind_stream)
            abind_stream->minusWatchers (delta);

        if (vbind_stream)
            vbind_stream->minusWatchers (delta);

        mutex.lock ();
    }

    num_watchers += delta;

    logD_ (_func, "calling fireNumWatchersChanged()");
    mt_async mt_unlocks_locks (mutex) fireNumWatchersChanged (num_watchers);
}

mt_async void
VideoStream::minusWatchers (Count const delta)
{
    if (delta == 0)
        return;

    mutex.lock ();
    mt_async mt_unlocks_locks (mutex) minusWatchers_unlocked (delta);
    mutex.unlock ();
}

mt_unlocks_locks (mutex) void
VideoStream::minusWatchers_unlocked (Count const delta)
{
    if (delta == 0)
        return;

    assert (num_watchers >= delta);
    num_watchers -= delta;

    // Keep in mind that bindToStream() can be called at any moment.
    // Calling bind_stream->minusWatchers() *after* changing 'num_watchers'
    // to avoid races.
    Ref<VideoStream> const abind_stream = abind.weak_bind_stream.getRef ();
    Ref<VideoStream> const vbind_stream = vbind.weak_bind_stream.getRef ();
    if (abind_stream || vbind_stream) {
        mutex.unlock ();

        if (abind_stream)
            mt_async abind_stream->minusWatchers (delta);

        if (vbind_stream)
            mt_async vbind_stream->minusWatchers (delta);

        mutex.lock ();
    }

    logD_ (_func, "calling fireNumWatchersChanged()");
    mt_unlocks_locks (mutex) fireNumWatchersChanged (num_watchers);
}

mt_mutex (mutex) bool
VideoStream::bind_messageBegin (BindInfo   * const mt_nonnull bind_info,
                                Message    * const mt_nonnull msg,
                                BindTicket * const bind_ticket,
                                bool         const is_audio_msg)
{
    bool const has_timestamp = msgHasTimestamp (msg, is_audio_msg);

    if (bind_ticket != bind_info->cur_bind_ticket) {
      // The message does not belong to the stream which we're currently bound to.

        if (bind_ticket != bind_info->pending_bind_ticket) {
          // Spurious message from some stream we used to be subscribed to before.
            logD_ (_this_func, "spurious message from some old stream (", (is_audio_msg ? "audio" : "video"), ")");
            return false;
        }

        logS_ (_this_func, "pending message");

      // The message belongs to the stream that we're about to switch to
      // when the right moment comes.

        if (is_audio_msg) {
            AudioMessage * const audio_msg = static_cast <AudioMessage*> (msg);
            PendingAudioFrame * const audio_frame = new PendingAudioFrame;
            audio_frame->audio_msg = *audio_msg;
            bind_info->pending_frame_list.append (audio_frame);
        } else {
            VideoMessage * const video_msg = static_cast <VideoMessage*> (msg);
            PendingVideoFrame * const video_frame = new PendingVideoFrame;
            video_frame->video_msg = *video_msg;
            bind_info->pending_frame_list.append (video_frame);
        }

        if (!bind_info->pending_got_timestamp_offs &&
            has_timestamp)
        {
            bind_info->pending_timestamp_offs = stream_timestamp_nanosec - msg->timestamp_nanosec;
            bind_info->pending_got_timestamp_offs = true;
        }

        return false;
    }

    // Варианты bind'а:
    //
    // Реально в lectoirum'е:
    //   * Аудио всегда от одного потока, видео - переключается.
    //     Т е это частичный бинд, "только видео".
    //
    // Потенциально:
    //   * Переключение видео/аудио на любой поток в произвольный момент.
    //
    //   1. Пустой поток. Включаем аудио и видео одновременно.
    //   2. Пустой поток. Включаем только аудио или только видео.
    //
    //   11 -> 00
    //   01 -> 00, 10 -> 00
    //
    //   00 -> 11
    //   00 -> 01, 00 -> 10
    //
    //   11 -> 01, 11 -> 10
    //
    //   01 -> 11, 10 -> 11
    //
    // _и_ существуют связанные и несвязанные потоки в случае "11":
    //
    //   00 -> aa
    //   00 -> ab
    //   0a -> aa 
    //   0b -> ab
    //   a0 -> aa
    //   a0 -> ab
    //
    // Кроме того, переключение выполняется асинхронно. В общем случае
    // кол-во привязанных потоков больше двух.
    //
    // Нужен компактный набор правил, чтобы минимизировать кол-во
    // вариантов.
    //
    // 1. Таймстемпы в потоке идут монотонно. Существует время потока,
    //    не привязанное к другим потокам. Таймстемпы привязанных потоков
    //    корректируются по дельте ко времени потока.
    //
    // 2. Каждый вновь привязанный поток корректируется по дельте
    //    ко времени потока.
    //
    // 3. Привязанные потоки, относящиеся к одному видеопотоку,
    //    имеют общие таймстемпы и корректируются совместно
    //    (по одинаковой дельте).
    //
    // Возможные события:
    //    * привязка потока A (или null);
    //    * отвязка потока (элементарное действие).
    //
    // При привязке потока вытесняется вытеснение предыдущего, но не сразу.
    // Т е существует (абстрактное) множество привязанных потоков (в не abind и vbind).

    if (has_timestamp) {
        if (!bind_info->got_timestamp_offs &&
            has_timestamp)
        {
            bind_info->timestamp_offs = stream_timestamp_nanosec - msg->timestamp_nanosec;
            bind_info->got_timestamp_offs = true;
        }
    }

    if (bind_info->got_timestamp_offs)
        stream_timestamp_nanosec = msg->timestamp_nanosec + bind_info->timestamp_offs;

#if 0
    last_adjusted_timestamp = msg->timestamp_nanosec + timestamp_offs;

    logD_ (_func, "msg->timestamp: 0x", fmt_hex, msg->timestamp_nanosec);
    logD_ (_func, "timestamp_offs: 0x", fmt_hex, timestamp_offs);
    logD_ (_func, "last_adjusted_timestamp: 0x", fmt_hex, last_adjusted_timestamp);
#endif

    ++bind_inform_counter;

    return true;
}

// TODO Move to VideoStream
struct BindFrameHandlerData
{
    VideoStream *self;
    VideoStream::BindInfo *bind_info;
    bool report_audio;
    bool report_video;
};

VideoStream::FrameSaver::FrameHandler const VideoStream::bind_frame_handler = {
    bind_savedAudioFrame,
    bind_savedVideoFrame
};

mt_unlocks_locks (mutex) Result
VideoStream::bind_savedAudioFrame (AudioMessage * const mt_nonnull audio_msg,
                                   void         * const _data)
{
    BindFrameHandlerData * const data = static_cast <BindFrameHandlerData*> (_data);
    VideoStream * const self = data->self;
    BindInfo * const bind_info = data->bind_info;

    logS_ (_self_func, audio_msg->frame_type);

    if (!data->report_audio) {
        logS_ (_self_func, "ignoring");
        return Result::Success;
    }

#if 0
    if (!bind_info->got_timestamp_offs
        && msgHasTimestamp (audio_msg, true /* is_audio_msg */))
    {
        bind_info->timestamp_offs = self->stream_timestamp_nanosec - audio_msg->timestamp_nanosec;
        bind_info->got_timestamp_offs = true;
    }

    if (bind_info->got_timestamp_offs)
        self->stream_timestamp_nanosec = audio_msg->timestamp_nanosec + bind_info->timestamp_offs;
#endif

    AudioMessage tmp_audio_msg = *audio_msg;
    tmp_audio_msg.timestamp_nanosec = self->stream_timestamp_nanosec;

    InformAudioMessage_Data inform_data (&tmp_audio_msg);
    mt_unlocks_locks (mutex) self->event_informer.informAll_unlocked (informAudioMessage, &inform_data);

    logS_ (_self_func, tmp_audio_msg.frame_type, ": to frame saver");
    self->frame_saver.processAudioFrame (&tmp_audio_msg);

    return Result::Success;
}

mt_unlocks_locks (mutex) Result
VideoStream::bind_savedVideoFrame (VideoMessage * const mt_nonnull video_msg,
                                   void         * const _data)
{
    BindFrameHandlerData * const data = static_cast <BindFrameHandlerData*> (_data);
    VideoStream * const self = data->self;
    BindInfo * const bind_info = data->bind_info;

    logS_ (_self_func, video_msg->frame_type);

    if (!data->report_video) {
        logS_ (_self_func, "ignoring");
        return Result::Success;
    }

#if 0
    if (!bind_info->got_timestamp_offs
        && msgHasTimestamp (video_msg, false /* is_audio_msg */))
    {
        bind_info->timestamp_offs = self->stream_timestamp_nanosec - video_msg->timestamp_nanosec;
        bind_info->got_timestamp_offs = true;
        logS_ (_self_func, "stream_timestamp_nanosec: ", self->stream_timestamp_nanosec);
        logS_ (_self_func, "video_msg->timestamp_nanosec: ", video_msg->timestamp_nanosec);
        logS_ (_self_func, "bind_info->timestamp_offs: ", bind_info->timestamp_offs);
    }

    if (bind_info->got_timestamp_offs)
        self->stream_timestamp_nanosec = video_msg->timestamp_nanosec + bind_info->timestamp_offs;
#endif

    VideoMessage tmp_video_msg = *video_msg;
    tmp_video_msg.timestamp_nanosec = self->stream_timestamp_nanosec;

    InformVideoMessage_Data inform_data (&tmp_video_msg);
    mt_unlocks_locks (mutex) self->event_informer.informAll_unlocked (informVideoMessage, &inform_data);

    self->frame_saver.processVideoFrame (&tmp_video_msg);

    return Result::Success;
}

mt_unlocks_locks (mutex) void
VideoStream::bind_doMessageEnd (BindInfo * const mt_nonnull bind_info)
{
    bind_info->cur_bind_ticket = bind_info->pending_bind_ticket;
    bind_info->pending_bind_ticket = NULL;

    frame_saver.releaseState (bind_info == &abind, bind_info == &vbind);
    FrameSaver tmp_frame_saver;
    tmp_frame_saver.copyStateFrom (&bind_info->pending_frame_saver);
    bind_info->pending_frame_saver.releaseState ();

    PendingFrameList tmp_frame_list = bind_info->pending_frame_list;
    bind_info->pending_frame_list.clear ();

    bool timestamp_set = false;
#if 0
// WRONG: this breaks bindToStream (stream, stream, true, true);
// TODO Do this properly.

    {
        Ref<VideoStream> const abind_stream = abind.weak_bind_stream.getRef();
        Ref<VideoStream> const vbind_stream = vbind.weak_bind_stream.getRef();
        if (abind_stream.ptr() == vbind_stream.ptr()) {
            logS_ (_this_func, "restoring timestamp_offs");
            if (bind_info == &abind) {
                if (vbind.got_timestamp_offs) {
                    logS_ (_this_func, "restoring timestamp_offs from vbind");
                    abind.timestamp_offs = vbind.timestamp_offs;
                    abind.got_timestamp_offs = true;
                    timestamp_set = true;
                }
            } else {
                assert (bind_info == &vbind);
                if (abind.got_timestamp_offs) {
                    logS_ (_this_func, "restoring timestamp_offs from abind");
                    vbind.timestamp_offs = abind.timestamp_offs;
                    vbind.got_timestamp_offs = true;
                    timestamp_set = true;
                }
            }
        }
    }
#endif

    if (!timestamp_set) {
        bind_info->timestamp_offs = bind_info->pending_timestamp_offs;
        bind_info->got_timestamp_offs = bind_info->pending_got_timestamp_offs;
    }

    bind_info->pending_timestamp_offs = 0;
    bind_info->pending_got_timestamp_offs = false;

  // TODO New messages should not be reported until bind_messageEnd() finishes reporting queued messages.
  //      Extra intermediate queue is needed to put messages to while reporting saved frames.

    {
        BindFrameHandlerData bind_handler_data;
        bind_handler_data.self = this;
        bind_handler_data.bind_info = bind_info;
        bind_handler_data.report_audio = (bind_info == &abind);
        bind_handler_data.report_video = (bind_info == &vbind);
        logS_ (_this_func, "calling reportSavedFrames()");
        mt_unlocks_locks (mutex) tmp_frame_saver.reportSavedFrames (&bind_frame_handler, &bind_handler_data);
        logS_ (_this_func, "reportSavedFrames() returned");
    }

    {
        PendingFrameList::iter frame_iter (tmp_frame_list);
        while (!tmp_frame_list.iter_done (frame_iter)) {
            PendingFrame * const pending_frame = tmp_frame_list.iter_next (frame_iter);
            switch (pending_frame->getType()) {
                case PendingFrame::t_Audio: {
                    PendingAudioFrame * const audio_frame = static_cast <PendingAudioFrame*> (pending_frame);
                    audio_frame->audio_msg.timestamp_nanosec += bind_info->timestamp_offs;
                    stream_timestamp_nanosec = audio_frame->audio_msg.timestamp_nanosec;

                    InformAudioMessage_Data inform_data (&audio_frame->audio_msg);
                    mt_unlocks_locks (mutex) event_informer.informAll_unlocked (informAudioMessage, &inform_data);

                    frame_saver.processAudioFrame (&audio_frame->audio_msg);
                } break;
                case PendingFrame::t_Video: {
                    PendingVideoFrame * const video_frame = static_cast <PendingVideoFrame*> (pending_frame);
                    video_frame->video_msg.timestamp_nanosec += bind_info->timestamp_offs;
                    stream_timestamp_nanosec = video_frame->video_msg.timestamp_nanosec;

                    InformVideoMessage_Data inform_data (&video_frame->video_msg);
                    mt_unlocks_locks (mutex) event_informer.informAll_unlocked (informVideoMessage, &inform_data);

                    frame_saver.processVideoFrame (&video_frame->video_msg);
                } break;
                default:
                    unreachable ();
            }

            delete pending_frame;
        }
    }
}

mt_unlocks_locks (mutex) void
VideoStream::bind_messageEnd ()
{
    if ((abind.pending_bind_ticket || vbind.pending_bind_ticket)
        && bind_inform_counter == 1)
    {
        logD_ (_this_func, "binding now");

        if (abind.pending_bind_ticket)
            bind_doMessageEnd (&abind);

        if (vbind.pending_bind_ticket)
            bind_doMessageEnd (&vbind);

        assert (bind_inform_counter > 0);
        --bind_inform_counter;
    } else {
        assert (bind_inform_counter > 0);
        --bind_inform_counter;
    }
}

VideoStream::EventHandler const VideoStream::abind_handler = {
    bind_audioMessage,
    NULL /* videoMessage */,
    NULL /* rtmpCommandMessage */,
    NULL /* closed */,
    NULL /* numWatchersChanged */
};

VideoStream::EventHandler const VideoStream::vbind_handler = {
    NULL /* audioMessage */,
    bind_videoMessage,
    NULL /* rtmpCommandMessage */,
    NULL /* closed */,
    NULL /* numWatchersChanged */
};

void
VideoStream::bind_audioMessage (AudioMessage * const mt_nonnull audio_msg,
                                void         * const _bind_ticket)
{
    BindTicket * const bind_ticket = static_cast <BindTicket*> (_bind_ticket);
    VideoStream * const self = bind_ticket->video_stream;

//    logS_ (_func, audio_msg->frame_type, ", ts: ", audio_msg->timestamp_nanosec);

    AudioMessage tmp_audio_msg = *audio_msg;

    self->mutex.lock ();
    if (!self->bind_messageBegin (&self->abind, audio_msg, bind_ticket, true /* is_audio_msg */)) {
        self->mutex.unlock ();
        return;
    }

    tmp_audio_msg.timestamp_nanosec += self->abind.timestamp_offs;
//    logS_ (_func, "timestamp: ", tmp_audio_msg.timestamp_nanosec);

    logS_ (_self_func,
           "ts: ", tmp_audio_msg.timestamp_nanosec, " (",
           audio_msg->timestamp_nanosec, " + ",
           (Int64) self->abind.timestamp_offs, ") ",
           audio_msg->frame_type);

    InformAudioMessage_Data inform_data (&tmp_audio_msg);
    mt_unlocks_locks (mutex) self->event_informer.informAll_unlocked (informAudioMessage, &inform_data);

    self->bind_messageEnd ();
    self->frame_saver.processAudioFrame (&tmp_audio_msg);

    self->mutex.unlock ();
}

void
VideoStream::bind_videoMessage (VideoMessage * const mt_nonnull video_msg,
                                void         * const _bind_ticket)
{
    BindTicket * const bind_ticket = static_cast <BindTicket*> (_bind_ticket);
    VideoStream * const self = bind_ticket->video_stream;

//    logS_ (_func, video_msg->frame_type, ", ts: ", video_msg->timestamp_nanosec);

    VideoMessage tmp_video_msg = *video_msg;

    self->mutex.lock ();
    if (!self->bind_messageBegin (&self->vbind, video_msg, bind_ticket, false /* is_audio_msg */)) {
        self->mutex.unlock ();
        return;
    }

    tmp_video_msg.timestamp_nanosec += self->vbind.timestamp_offs;
//    logS_ (_func, "timestamp: ", tmp_video_msg.timestamp_nanosec);

    logS_ (_self_func, "ts: ",
           tmp_video_msg.timestamp_nanosec, " (",
           video_msg->timestamp_nanosec, " + ",
           (Int64) self->vbind.timestamp_offs, ") ",
           video_msg->frame_type);

    InformVideoMessage_Data inform_data (&tmp_video_msg);
    mt_unlocks_locks (mutex) self->event_informer.informAll_unlocked (informVideoMessage, &inform_data);

    self->bind_messageEnd ();
    self->frame_saver.processVideoFrame (&tmp_video_msg);

    self->mutex.unlock ();
}

void
VideoStream::bind_rtmpCommandMessage (RtmpConnection    * const mt_nonnull /* conn */,
                                      Message           * const mt_nonnull /* msg */,
                                      ConstMemory const & /* method_name */,
                                      AmfDecoder        * const mt_nonnull /* amf_decoder */,
                                      void              * const /* _bind_ticket */)
{
    logW_ (_func, "message dropped");

#if 0
// TODO Propagate command messages.

    BindTicket * const bind_ticket = static_cast <BindTicket*> (_bind_ticket);
    VideoStream * const self = bind_ticket->video_stream;

    self->fireRtmpCommandMessage (conn, msg, method_name, amf_decoder);
#endif
}

void
VideoStream::bindToStream (VideoStream * const bind_audio_stream,
                           VideoStream * const bind_video_stream,
                           bool         bind_audio,
                           bool         bind_video)
{
  // Note: Binding two streams to one another doesn't make _any_ sense (!)
  //   => it *will* break anyway in this case
  //   => we can safely assume that no two streams
  //      will cause lock inversion when binding.

    logD_ (_this_func, "bind_audio: ", bind_audio, ", bind_video: ", bind_video);
    logD_ (_this_func, "bind_audio_stream: 0x", fmt_hex, (UintPtr) bind_audio_stream);
    logD_ (_this_func, "bind_video_stream: 0x", fmt_hex, (UintPtr) bind_video_stream);

    if (!bind_video && !bind_audio) {
        logD_ (_this_func, "no-op");
        return;
    }

// TODO Bind ops cannot overlap, BUT recursive calls are possible from the client.
//      Lock 'mutex' first, then check for the streams currently being bound and
//      remember the new requisition, if different. Loop at the end of bindToStream().
//      'bind_mutex' will become unnecessary after that and should go away.

    bind_mutex.lock ();

    // Cannot bind to self.
    assert (bind_audio_stream != this);
    assert (bind_video_stream != this);

    mutex.lock ();

    Ref<VideoStream> const prv_abind_stream = abind.weak_bind_stream.getRef();
    if (prv_abind_stream == bind_audio_stream) {
      // No-op if re-binding to the same stream.
        bind_audio = false;
    }

    Ref<VideoStream> const prv_vbind_stream = vbind.weak_bind_stream.getRef();
    if (prv_vbind_stream == bind_video_stream) {
      // No-op if re-binding to the same stream.
        bind_video = false;
    }

    GenericInformer::SubscriptionKey const prv_abind_sbn = abind.bind_sbn;
    GenericInformer::SubscriptionKey const prv_vbind_sbn = vbind.bind_sbn;

    if (!bind_audio && !bind_video) {
        logD_ (_this_func, "no-op after self-test");
        mutex.unlock ();
        bind_mutex.unlock ();
        return;
    }

    logD_ (_this_func, "after self-test: bind_audio: ", bind_audio, ", bind_video: ", bind_video);

#if 0
// WRONG

    if (bind_audio && !bind_video && bind_audio_stream == prv_vbind_stream) {
      // TODO Shortcut for audio binding
        mutex.unlock ();
        return;
    }

    if (bind_video && !bind_audio && bind_video_stream == prv_abind_stream) {
      // TODO Shortcut for video binding
        mutex.unlock ();
        return;
    }
#endif

    // Avoiding overlapping with message reports for the old stream.
    ++bind_inform_counter;
    logD_ (_this_func, "new bind_inform_counter: ", bind_inform_counter);

#if 0
// Unnecessary

    if (bind_video || bind_audio) {
        Count tmp_num_watchers = num_watchers;
        Count reported_num_watchers = 0;
        while (tmp_num_watchers != reported_num_watchers) {
            mutex.unlock ();

            if (reported_num_watchers < tmp_num_watchers) {
                Count const delta = tmp_num_watchers - reported_num_watchers;

                if (bind_audio && bind_audio_stream)
                    bind_audio_stream->plusWatchers (delta);

                if (bind_video && bind_video_stream)
                    bind_video_stream->plusWatchers (delta);
            } else {
                Count const delta = reported_num_watchers - tmp_num_watchers;

                if (bind_audio && bind_audio_stream)
                    bind_audio_stream->minusWatchers (delta);

                if (bind_video && bind_video_stream)
                    bind_video_stream->minusWatchers (delta);
            }

            mutex.lock ();

            tmp_num_watchers = num_watchers;
        }
    }
#endif

    if (bind_audio) {
        if (bind_audio_stream) {
            abind.weak_bind_stream = bind_audio_stream;

            Ref<BindTicket> const bind_ticket = grab (new BindTicket);
            bind_ticket->video_stream = this;
            bind_ticket->bind_stream = bind_audio_stream;

            abind.pending_timestamp_offs = 0;
            abind.pending_got_timestamp_offs = false;
            abind.pending_bind_ticket = bind_ticket;

            bind_audio_stream->mutex.lock ();
            bind_audio_stream->plusWatchers_unlocked (num_watchers);
            abind.pending_frame_saver.copyStateFrom (&bind_audio_stream->frame_saver);
            logD_ (_this_func, "subscribing for audio messages");
            abind.bind_sbn = bind_audio_stream->getEventInformer()->subscribe_unlocked (CbDesc<EventHandler> (
                                    &abind_handler, bind_ticket, this, bind_ticket));
            bind_audio_stream->mutex.unlock ();
        } else {
            abind.reset ();
        }
    }

    if (bind_video) {
        if (bind_video_stream) {
            vbind.weak_bind_stream = bind_video_stream;

            Ref<BindTicket> const bind_ticket = grab (new BindTicket);
            bind_ticket->video_stream = this;
            bind_ticket->bind_stream = bind_video_stream;

            vbind.pending_timestamp_offs = 0;
            vbind.pending_got_timestamp_offs = false;
            vbind.pending_bind_ticket = bind_ticket;

            bind_video_stream->mutex.lock ();
            bind_video_stream->plusWatchers_unlocked (num_watchers);
            vbind.pending_frame_saver.copyStateFrom (&bind_video_stream->frame_saver);
            logD_ (_this_func, "subscribing for video messages");
            vbind.bind_sbn = bind_video_stream->getEventInformer()->subscribe_unlocked (CbDesc<EventHandler> (
                                     &vbind_handler, bind_ticket, this, bind_ticket));
            bind_video_stream->mutex.unlock ();
        } else {
            vbind.reset ();
        }
    }

    if (bind_audio && prv_abind_stream) {
        prv_abind_stream->lock ();
        if (prv_abind_sbn)
            prv_abind_stream->getEventInformer()->unsubscribe_unlocked (prv_abind_sbn);
        prv_abind_stream->minusWatchers_unlocked (num_watchers);
        prv_abind_stream->unlock ();
    }

    // TODO Don't hurry to unsubscribe (keyframe awaiting logics for instant transition).
    if (bind_video && prv_vbind_stream) {
        prv_vbind_stream->lock ();
        if (prv_vbind_sbn)
            prv_vbind_stream->getEventInformer()->unsubscribe_unlocked (prv_vbind_sbn);
        prv_vbind_stream->minusWatchers_unlocked (num_watchers);
        prv_vbind_stream->unlock ();
    }

    if (bind_audio ||
        bind_video)
    {
        bind_messageEnd ();
    }

    mutex.unlock ();

    bind_mutex.unlock ();
}

void
VideoStream::close ()
{
    mutex.lock ();
    is_closed = true;
    mt_unlocks_locks (mutex) event_informer.informAll_unlocked (informClosed, NULL /* inform_data */);
    mutex.unlock ();
}

VideoStream::VideoStream ()
    : is_closed (false),
      num_watchers (0),

      stream_timestamp_nanosec (0),

#if 0
      timestamp_offs (0),
      last_adjusted_timestamp (0),
      got_timestamp_offs (false),
#endif

//      full_bind (false),
      bind_inform_counter (0),

      event_informer (this, &mutex)
{
    logD_ (_this_func_);
}

VideoStream::~VideoStream ()
{
    logD_ (_this_func_);

  // This lock ensures data consistency for 'frame_saver's destructor.
  // TODO ^^^ Does it? A single mutex lock/unlock pair does not (ideally) constitute
  //      a full memory barrier.
  StateMutexLock l (&mutex);

    {
        Ref<VideoStream> const abind_stream = abind.weak_bind_stream.getRef ();
        Ref<VideoStream> const vbind_stream = vbind.weak_bind_stream.getRef ();
        if (abind_stream ||
            vbind_stream)
        {
            Count const tmp_num_watchers = num_watchers;
            mutex.unlock ();

            if (abind_stream)
                abind_stream->minusWatchers (tmp_num_watchers);

            if (vbind_stream)
                vbind_stream->minusWatchers (tmp_num_watchers);

            mutex.lock ();
        }
    }
}

}

