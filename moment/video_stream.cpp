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

namespace {
LogGroup libMary_logGroup_frame_saver ("frame_saver", LogLevel::N);
}

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
	case KeyFrame:
	    return 1;
	case InterFrame:
	    return 2;
	case DisposableInterFrame:
	    return 3;
	case GeneratedKeyFrame:
	    return 4;
	case CommandFrame:
	case AvcSequenceHeader:
	case AvcEndOfSequence:
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

	    logD_ (_func, "AAC SEQUENCE HEADER");

	    if (got_saved_aac_seq_hdr)
		saved_aac_seq_hdr.msg.page_pool->msgUnref (saved_aac_seq_hdr.msg.page_list.first);

	    got_saved_aac_seq_hdr = true;
	    saved_aac_seq_hdr.msg = *msg;

	    msg->page_pool->msgRef (msg->page_list.first);
	} break;
	default:
	  // No-op
	    ;
    }
}

void
VideoStream::FrameSaver::processVideoFrame (VideoMessage * const mt_nonnull msg)
{
    logD (frame_saver, _func, "0x", fmt_hex, (UintPtr) this);

    switch (msg->frame_type) {
	case VideoFrameType::KeyFrame: {
	    logD (frame_saver, _func, msg->frame_type);

	    if (got_saved_keyframe)
		saved_keyframe.msg.page_pool->msgUnref (saved_keyframe.msg.page_list.first);

	    got_saved_keyframe = true;
	    saved_keyframe.msg = *msg;

	    msg->page_pool->msgRef (msg->page_list.first);
	} break;
	case VideoFrameType::AvcSequenceHeader: {
	    logD (frame_saver, _func, msg->frame_type);

	    if (got_saved_avc_seq_hdr)
		saved_avc_seq_hdr.msg.page_pool->msgUnref (saved_avc_seq_hdr.msg.page_list.first);

	    got_saved_avc_seq_hdr = true;
	    saved_avc_seq_hdr.msg = *msg;

	    msg->page_pool->msgRef (msg->page_list.first);
	} break;
	case VideoFrameType::AvcEndOfSequence: {
	    logD (frame_saver, _func, msg->frame_type);

	    if (got_saved_avc_seq_hdr)
		saved_avc_seq_hdr.msg.page_pool->msgUnref (saved_avc_seq_hdr.msg.page_list.first);

	    got_saved_avc_seq_hdr = false;
	} break;
	case VideoFrameType::RtmpSetMetaData: {
	    logD (frame_saver, _func, msg->frame_type);

	    if (got_saved_metadata)
		saved_metadata.msg.page_pool->msgUnref (saved_metadata.msg.page_list.first);

	    got_saved_metadata = true;
	    saved_metadata.msg = *msg;

	    msg->page_pool->msgRef (msg->page_list.first);
	} break;
	case VideoFrameType::RtmpClearMetaData: {
	    logD (frame_saver, _func, msg->frame_type);

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
VideoStream::FrameSaver::getSavedKeyframe (SavedFrame * const mt_nonnull ret_frame)
{
    if (!got_saved_keyframe)
	return false;

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

VideoStream::FrameSaver::FrameSaver ()
    : got_saved_keyframe (false),
      got_saved_metadata (false),
      got_saved_aac_seq_hdr (false),
      got_saved_avc_seq_hdr (false)
{
}

VideoStream::FrameSaver::~FrameSaver ()
{
    if (got_saved_keyframe)
	saved_keyframe.msg.page_pool->msgUnref (saved_keyframe.msg.page_list.first);

    if (got_saved_metadata)
	saved_metadata.msg.page_pool->msgUnref (saved_metadata.msg.page_list.first);

    if (got_saved_aac_seq_hdr)
	saved_aac_seq_hdr.msg.page_pool->msgUnref (saved_aac_seq_hdr.msg.page_list.first);

    if (got_saved_avc_seq_hdr)
	saved_avc_seq_hdr.msg.page_pool->msgUnref (saved_avc_seq_hdr.msg.page_list.first);
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
    InformAudioMessage_Data * const inform_data = static_cast <InformAudioMessage_Data*> (_inform_data);
    event_handler->audioMessage (inform_data->msg, cb_data);
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
    InformVideoMessage_Data * const inform_data = static_cast <InformVideoMessage_Data*> (_inform_data);
    event_handler->videoMessage (inform_data->msg, cb_data);
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
    InformRtmpCommandMessage_Data * const inform_data =
	    static_cast <InformRtmpCommandMessage_Data*> (_inform_data);
    // TODO Save/restore amf_decoder state between  callback invocations.
    //      Viable option - abstract away the parsing process.
    event_handler->rtmpCommandMessage (inform_data->conn,
				       inform_data->msg,
				       inform_data->method_name,
				       inform_data->amf_decoder,
				       cb_data);
}

void
VideoStream::informClosed (EventHandler * const event_handler,
			   void * const cb_data,
			   void * const /* inform_data */)
{
    event_handler->closed (cb_data);
}

void
VideoStream::fireAudioMessage (AudioMessage * const mt_nonnull msg)
{
//    logD_ (_func, "timestamp: 0x", fmt_hex, (UintPtr) msg_info->timestamp);

    mutex.lock ();

    frame_saver.processAudioFrame (msg);

    InformAudioMessage_Data inform_data (msg);
    event_informer.informAll_unlocked (informAudioMessage, &inform_data);

    mutex.unlock ();
}

void
VideoStream::fireVideoMessage (VideoMessage * const mt_nonnull msg)
{
//    logD_ (_func, "timestamp: 0x", fmt_hex, (UintPtr) msg->timestamp);

    mutex.lock ();

    frame_saver.processVideoFrame (msg);

    InformVideoMessage_Data inform_data (msg);
    event_informer.informAll_unlocked (informVideoMessage, &inform_data);

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

void
VideoStream::close ()
{
    event_informer.informAll (informClosed, NULL /* inform_data */);
}

VideoStream::VideoStream ()
    : event_informer (this, &mutex)
{
}

VideoStream::~VideoStream ()
{
  // This lock ensures data consistency for 'frame_saver's destructor.
  StateMutexLock l (&mutex);
}

}

