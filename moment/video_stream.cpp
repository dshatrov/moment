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
					Format const & /* fmt */)
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
					Format const & /* fmt */)
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
				      Format const & /* fmt */)
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
				      Format const & /* fmt */)
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

bool
VideoStream::FrameSaver::getSavedKeyframe (SavedFrame * const ret_frame)
{
    if (!got_saved_keyframe)
	return false;

    if (ret_frame)
	*ret_frame = saved_keyframe;

    return true;
}

bool
VideoStream::FrameSaver::getSavedMetaData (SavedFrame * const mt_nonnull ret_frame)
{
    if (!got_saved_metadata)
	return false;

    *ret_frame = saved_metadata;
    return true;
}

bool
VideoStream::FrameSaver::getSavedAacSeqHdr (SavedAudioFrame * const mt_nonnull ret_frame)
{
    if (!got_saved_aac_seq_hdr)
	return false;

    *ret_frame = saved_aac_seq_hdr;
    return true;
}

bool
VideoStream::FrameSaver::getSavedAvcSeqHdr (SavedFrame * const mt_nonnull ret_frame)
{
    if (!got_saved_avc_seq_hdr)
	return false;

    *ret_frame = saved_avc_seq_hdr;
    return true;
}

Size
VideoStream::FrameSaver::getNumSavedSpeexHeaders ()
{
    return saved_speex_headers.getNumElements();
}

void
VideoStream::FrameSaver::getSavedSpeexHeaders (SavedAudioFrame *ret_frames,
					       Size             ret_frame_size)
{
    Size i = 0;
    List<SavedAudioFrame*>::iter iter (saved_speex_headers);
    while (!saved_speex_headers.iter_done (iter)) {
	if (i >= ret_frame_size)
	    break;

	SavedAudioFrame * const frame = saved_speex_headers.iter_next (iter)->data;
	ret_frames [i] = *frame;

	++i;
    }
}

void
VideoStream::FrameSaver::copyStateFrom (FrameSaver * const frame_saver)
{
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
        VideoStream::VideoMessage msg;

        msg.timestamp_nanosec = saved_avc_seq_hdr.msg.timestamp_nanosec;
        msg.codec_id = VideoStream::VideoCodecId::AVC;
        msg.frame_type = VideoStream::VideoFrameType::AvcEndOfSequence;

        msg.page_pool = saved_avc_seq_hdr.msg.page_pool;
        msg.prechunk_size = 0;
        msg.msg_offset = 0;

        msg.is_saved_frame = true;

      // TODO Send AvcEndOfSequence only when AvcSequenceHeader was sent.
        Byte avc_video_hdr [5] = { 0x17, 2, 0, 0, 0 }; // AVC, seekable frame;
                                                       // AVC end of sequence;
                                                       // Composition time offset = 0.

        // TODO FIXME This should be done in mod_rtmp.
        msg.page_pool->getFillPages (&msg.page_list, ConstMemory::forObject (avc_video_hdr));

        msg.msg_len = sizeof (avc_video_hdr);

        if (!frame_handler->videoFrame (&msg, cb_data))
            return Result::Failure;
    }

    if (got_saved_avc_seq_hdr) {
        if (frame_handler->videoFrame) {
            if (!frame_handler->videoFrame (&saved_avc_seq_hdr.msg, cb_data))
                return Result::Failure;
        }
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
}

void
VideoStream::FrameSaver::releaseState ()
{
    if (got_saved_keyframe)
	saved_keyframe.msg.page_pool->msgUnref (saved_keyframe.msg.page_list.first);

    if (got_saved_metadata)
	saved_metadata.msg.page_pool->msgUnref (saved_metadata.msg.page_list.first);

    if (got_saved_aac_seq_hdr)
	saved_aac_seq_hdr.msg.page_pool->msgUnref (saved_aac_seq_hdr.msg.page_list.first);

    if (got_saved_avc_seq_hdr)
	saved_avc_seq_hdr.msg.page_pool->msgUnref (saved_avc_seq_hdr.msg.page_list.first);

    releaseSavedInterframes ();
    releaseSavedSpeexHeaders ();
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
    // TODO Save/restore amf_decoder state between  callback invocations.
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
//    logD_ (_func, "timestamp: 0x", fmt_hex, (UintPtr) msg_info->timestamp);

    mutex.lock ();

    InformAudioMessage_Data inform_data (msg);
    mt_unlocks_locks (mutex) event_informer.informAll_unlocked (informAudioMessage, &inform_data);

    frame_saver.processAudioFrame (msg);

    mutex.unlock ();
}

void
VideoStream::fireVideoMessage (VideoMessage * const mt_nonnull msg)
{
//    logD_ (_func, "timestamp: 0x", fmt_hex, (UintPtr) msg->timestamp);

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
    Ref<VideoStream> const bind_stream = weak_bind_stream.getRef ();
    if (bind_stream) {
        mutex.unlock ();
        mt_async bind_stream->plusOneWatcher ();
        mutex.lock ();
    }

    ++num_watchers;

    logD_ (_func, "calling fireNumWatchersChanged()");
    mt_async mt_unlocks_locks (mutex) fireNumWatchersChanged (num_watchers);

    if (guard_obj)
        guard_obj->addDeletionCallback (watcherDeletionCallback,
                                        this /* cb_data */,
                                        NULL /* ref_data */,
                                        this /* guard_obj */);
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
    Ref<VideoStream> const bind_stream = weak_bind_stream.getRef ();
    if (bind_stream) {
        mutex.unlock ();
        mt_async bind_stream->minusOneWatcher ();
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
    // Calling bind_stream->plusWatchers() *before* changing 'num_watchers'
    // to avoid races.
    Ref<VideoStream> const bind_stream = weak_bind_stream.getRef ();
    if (bind_stream) {
        mutex.unlock ();
        mt_async bind_stream->plusWatchers (delta);
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
    Ref<VideoStream> const bind_stream = weak_bind_stream.getRef ();
    if (bind_stream) {
        mutex.unlock ();
        mt_async bind_stream->minusWatchers (delta);
        mutex.lock ();
    }

    logD_ (_func, "calling fireNumWatchersChanged()");
    mt_unlocks_locks (mutex) fireNumWatchersChanged (num_watchers);
}

bool
VideoStream::bind_messageBegin (Message    * const mt_nonnull msg,
                                BindTicket * const bind_ticket,
                                bool         const is_audio_msg,
                                Uint64     * const mt_nonnull ret_timestamp_offs)
{
    mutex.lock ();
    if (bind_ticket != cur_bind_ticket) {
      // The message does not belong to the stream which we're currently bound to.

        if (bind_ticket != pending_bind_ticket) {
          // Spurious message from some stream we used to be subscribed to before.
            mutex.unlock ();
            logD_ (_func, "spurious message from some old stream");
            return false;
        }

      // The messages belongs to the stream that we're about to switch to
      // once the right moment comes.

        if (is_audio_msg) {
            AudioMessage * const audio_msg = static_cast <AudioMessage*> (msg);
            PendingAudioFrame * const audio_frame = new PendingAudioFrame;
            audio_frame->audio_msg = *audio_msg;
            pending_frame_list.append (audio_frame);
        } else {
            VideoMessage * const video_msg = static_cast <VideoMessage*> (msg);
            PendingVideoFrame * const video_frame = new PendingVideoFrame;
            video_frame->video_msg = *video_msg;
            pending_frame_list.append (video_frame);
        }

        if (!pending_got_timestamp_offs
            && msg->timestamp_nanosec > 0)
        {
            pending_timestamp_offs = -msg->timestamp_nanosec;
            pending_got_timestamp_offs = true;
        }

        mutex.unlock ();
        return false;
    }

    if (!got_timestamp_offs
        && msg->timestamp_nanosec > 0)
    {
        timestamp_offs -= msg->timestamp_nanosec;
        got_timestamp_offs = true;
//        logD_ (_func, "updated timestamp_offs: 0x", fmt_hex, timestamp_offs);
    }

    last_adjusted_timestamp = msg->timestamp_nanosec + timestamp_offs;
#if 0
    logD_ (_func, "msg->timestamp: 0x", fmt_hex, msg->timestamp_nanosec);
    logD_ (_func, "timestamp_offs: 0x", fmt_hex, timestamp_offs);
    logD_ (_func, "last_adjusted_timestamp: 0x", fmt_hex, last_adjusted_timestamp);
#endif

    *ret_timestamp_offs = timestamp_offs;

    ++bind_inform_counter;
    mutex.unlock ();
    return true;
}

VideoStream::FrameSaver::FrameHandler const VideoStream::bind_frame_handler = {
    bind_savedAudioFrame,
    bind_savedVideoFrame
};

mt_unlocks_locks (mutex) Result
VideoStream::bind_savedAudioFrame (AudioMessage * const mt_nonnull audio_msg,
                                   void         * const _self)
{
//    logD_ (_func, "ts 0x", fmt_hex, audio_msg->timestamp_nanosec);

    VideoStream * const self = static_cast <VideoStream*> (_self);

    if (!self->got_timestamp_offs
        && audio_msg->timestamp_nanosec > 0)
    {
        self->timestamp_offs -= audio_msg->timestamp_nanosec;
        self->got_timestamp_offs = true;
//        logD_ (_func, "updated timestamp_offs: 0x", fmt_hex, self->timestamp_offs);
    }

    self->last_adjusted_timestamp =
            audio_msg->timestamp_nanosec + self->timestamp_offs /* = previous value of 'timestamp_offs' */;
    Uint64 const tmp_timestamp = self->last_adjusted_timestamp;

    AudioMessage tmp_audio_msg = *audio_msg;
    tmp_audio_msg.timestamp_nanosec = tmp_timestamp;

    InformAudioMessage_Data inform_data (&tmp_audio_msg);
    mt_unlocks_locks (mutex) self->event_informer.informAll_unlocked (informAudioMessage, &inform_data);

    return Result::Success;
}

mt_unlocks_locks (mutex) Result
VideoStream::bind_savedVideoFrame (VideoMessage * const mt_nonnull video_msg,
                                   void         * const _self)
{
//    logD_ (_func, "ts 0x", fmt_hex, video_msg->timestamp_nanosec);

    VideoStream * const self = static_cast <VideoStream*> (_self);

    if (!self->got_timestamp_offs
        && video_msg->timestamp_nanosec > 0)
    {
        self->timestamp_offs -= video_msg->timestamp_nanosec;
        self->got_timestamp_offs = true;
//        logD_ (_func, "updated timestamp_offs: 0x", fmt_hex, self->timestamp_offs);
    }

    self->last_adjusted_timestamp =
            video_msg->timestamp_nanosec + self->timestamp_offs /* = previous value of 'timestamp_offs' */;
    Uint64 const tmp_timestamp = self->last_adjusted_timestamp;

    VideoMessage tmp_video_msg = *video_msg;
    tmp_video_msg.timestamp_nanosec = tmp_timestamp;

    InformVideoMessage_Data inform_data (&tmp_video_msg);
    mt_unlocks_locks (mutex) self->event_informer.informAll_unlocked (informVideoMessage, &inform_data);

    return Result::Success;
}

mt_unlocks_locks (mutex) void
VideoStream::bind_messageEnd ()
{
    if (pending_bind_ticket
        && bind_inform_counter == 1)
    {
        cur_bind_ticket = pending_bind_ticket;
        pending_bind_ticket = NULL;

        frame_saver.copyStateFrom (&pending_frame_saver);

        PendingFrameList tmp_frame_list = pending_frame_list;
        pending_frame_list.clear ();

        logD_ (_func, "pending_timestamp_offs: 0x", fmt_hex, pending_timestamp_offs);
        logD_ (_func, "last_adjusted_timestamp: 0x", fmt_hex, last_adjusted_timestamp);

        timestamp_offs = pending_timestamp_offs + last_adjusted_timestamp;
        got_timestamp_offs = pending_got_timestamp_offs;

        logD_ (_func, "timestamp_offs: 0x", fmt_hex, timestamp_offs, ", "
               "got_timestamp_offs: ", got_timestamp_offs);

        pending_timestamp_offs = 0;
        pending_got_timestamp_offs = false;

      // TODO New messages should not be reported until bind_messageEnd() finishes reporting queued messages.
      //      Extra intermediate queue is needed to put messages to while reporting saved frames.

        mt_unlocks_locks (mutex) frame_saver.reportSavedFrames (&bind_frame_handler, this);

        {
            PendingFrameList::iter frame_iter (tmp_frame_list);
            while (!tmp_frame_list.iter_done (frame_iter)) {
                PendingFrame * const pending_frame = tmp_frame_list.iter_next (frame_iter);
                switch (pending_frame->getType()) {
                    case PendingFrame::t_Audio: {
                        PendingAudioFrame * const audio_frame = static_cast <PendingAudioFrame*> (pending_frame);
                        audio_frame->audio_msg.timestamp_nanosec += timestamp_offs;
                        last_adjusted_timestamp = audio_frame->audio_msg.timestamp_nanosec;

                        InformAudioMessage_Data inform_data (&audio_frame->audio_msg);
                        mt_unlocks_locks (mutex) event_informer.informAll_unlocked (informAudioMessage, &inform_data);

                        frame_saver.processAudioFrame (&audio_frame->audio_msg);
                    } break;
                    case PendingFrame::t_Video: {
                        PendingVideoFrame * const video_frame = static_cast <PendingVideoFrame*> (pending_frame);
                        video_frame->video_msg.timestamp_nanosec += timestamp_offs;
                        last_adjusted_timestamp = video_frame->video_msg.timestamp_nanosec;

                        InformVideoMessage_Data inform_data (&video_frame->video_msg);
                        mt_unlocks_locks (mutex) event_informer.informAll_unlocked (informVideoMessage, &inform_data);

                        frame_saver.processVideoFrame (&video_frame->video_msg);
                    } break;
                    default:
                        unreachable ();
                }
            }
        }

        assert (bind_inform_counter > 0);
        --bind_inform_counter;
    } else {
        assert (bind_inform_counter > 0);
        --bind_inform_counter;
    }
}

VideoStream::EventHandler const VideoStream::bind_handler = {
    bind_audioMessage,
    bind_videoMessage,
    bind_rtmpCommandMessage,
    NULL /* closed */,
    NULL /* numWatchersChanged */
};

void
VideoStream::bind_audioMessage (AudioMessage * const mt_nonnull audio_msg,
                                void         * const _bind_ticket)
{
    BindTicket * const bind_ticket = static_cast <BindTicket*> (_bind_ticket);
    VideoStream * const self = bind_ticket->video_stream;

//    logD_ (_func, audio_msg->frame_type);

    Uint64 tmp_timestamp_offs;
    if (!self->bind_messageBegin (audio_msg, bind_ticket, true /* is_audio_msg */, &tmp_timestamp_offs))
        return;

    AudioMessage tmp_audio_msg = *audio_msg;
    tmp_audio_msg.timestamp_nanosec += tmp_timestamp_offs;

    logD_ (_func, "timestamp: ", tmp_audio_msg.timestamp_nanosec);

    self->mutex.lock ();

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

//    logD_ (_func, video_msg->frame_type);

    Uint64 tmp_timestamp_offs;
    if (!self->bind_messageBegin (video_msg, bind_ticket, false /* is_audio_msg */, &tmp_timestamp_offs))
        return;

    VideoMessage tmp_video_msg = *video_msg;
    tmp_video_msg.timestamp_nanosec += tmp_timestamp_offs;

    logD_ (_func, "timestamp: ", tmp_video_msg.timestamp_nanosec);

    self->mutex.lock ();

    InformVideoMessage_Data inform_data (&tmp_video_msg);
    mt_unlocks_locks (mutex) self->event_informer.informAll_unlocked (informVideoMessage, &inform_data);

    self->bind_messageEnd ();
    self->frame_saver.processVideoFrame (&tmp_video_msg);

    self->mutex.unlock ();
}

void
VideoStream::bind_rtmpCommandMessage (RtmpConnection    * const mt_nonnull conn,
                                      Message           * const mt_nonnull msg,
                                      ConstMemory const &method_name,
                                      AmfDecoder        * const mt_nonnull amf_decoder,
                                      void              * const _self)
{
    VideoStream * const self = static_cast <VideoStream*> (_self);
    self->fireRtmpCommandMessage (conn, msg, method_name, amf_decoder);
}

void
VideoStream::bindToStream (VideoStream * const bind_stream)
{
    bind_mutex.lock ();

    // Cannot bind to self.
    assert (bind_stream != this);

    Ref<BindTicket> const bind_ticket = grab (new BindTicket);
    bind_ticket->video_stream = this;
    bind_ticket->bind_stream = bind_stream;

    mutex.lock ();

    Ref<VideoStream> const prv_bind_stream = weak_bind_stream.getRef ();
    if (prv_bind_stream == bind_stream) {
      // No-op if re-binding to the same stream.
        mutex.unlock ();
        bind_mutex.unlock ();
        return;
    }

    weak_bind_stream = bind_stream;

    Count const tmp_num_watchers = num_watchers;

    pending_timestamp_offs = 0;
    pending_got_timestamp_offs = false;

    pending_bind_ticket = bind_ticket;
    ++bind_inform_counter;

    mutex.unlock ();

    // TODO Don't hurry to unsubscribe (keyframe awaiting logics for instant transition).
    if (prv_bind_stream) {
        prv_bind_stream->getEventInformer()->unsubscribe (bind_sbn);
        prv_bind_stream->minusWatchers (tmp_num_watchers);
    }

    if (!bind_stream) {
        bind_mutex.unlock ();
        return;
    }

    bind_stream->plusWatchers (num_watchers);

    FrameSaver tmp_frame_saver;

    bind_stream->lock ();
    tmp_frame_saver.copyStateFrom (&bind_stream->frame_saver);
    GenericInformer::SubscriptionKey const tmp_bind_sbn =
            bind_stream->getEventInformer()->subscribe_unlocked (CbDesc<EventHandler> (
                    &bind_handler, bind_ticket, this, bind_ticket));
    bind_stream->unlock ();

  // TODO What if an a/v message arrives at this moment? pending_frame_saver not initialized?
  //      Probably 'pending_bind_ticket' init from above should go below.

    mutex.lock ();
    bind_sbn = tmp_bind_sbn;
    pending_frame_saver.copyStateFrom (&tmp_frame_saver);
    bind_messageEnd ();
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
      bind_inform_counter (0),

      timestamp_offs (0),
      last_adjusted_timestamp (0),
      got_timestamp_offs (false),

      pending_timestamp_offs (0),
      pending_got_timestamp_offs (false),

      event_informer (this, &mutex)
{
}

VideoStream::~VideoStream ()
{
  // This lock ensures data consistency for 'frame_saver's destructor.
  // TODO ^^^ Does it? A single mutex lock/unlock pair does not (ideally) constitute
  //      a full memory barrier.
  StateMutexLock l (&mutex);

    {
        Ref<VideoStream> const bind_stream = weak_bind_stream.getRef ();
        if (bind_stream) {
            Count const tmp_num_watchers = num_watchers;
            mutex.unlock ();
            bind_stream->minusWatchers (tmp_num_watchers);
            mutex.lock ();
        }
    }
}

}

