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
VideoStream::FrameSaver::processAudioFrame (AudioMessageInfo       * const mt_nonnull msg_info,
					    PagePool               * const mt_nonnull page_pool,
					    PagePool::PageListHead * const mt_nonnull page_list,
					    Size                     const msg_len,
					    Size                     const msg_offset)
{
    logD (frame_saver, _func, "0x", fmt_hex, (UintPtr) this);

#if 0
    if (page_list->first && page_list->first->data_len >= 1)
	logD_ (_func, "audio header: 0x", fmt_hex, (unsigned) page_list->first->getData() [0]);
#endif

    switch (msg_info->frame_type) {
	case AudioFrameType::AacSequenceHeader: {
	    logD (frame_saver, _func, msg_info->frame_type);

	    logD_ (_func, "AAC SEQUENCE HEADER");

	    if (got_saved_aac_seq_hdr)
		saved_aac_seq_hdr.page_pool->msgUnref (saved_aac_seq_hdr.page_list.first);

	    got_saved_aac_seq_hdr = true;
	    saved_aac_seq_hdr.msg_info = *msg_info;
	    saved_aac_seq_hdr.page_pool = page_pool;
	    saved_aac_seq_hdr.page_list = *page_list;
	    saved_aac_seq_hdr.msg_len = msg_len;
	    saved_aac_seq_hdr.msg_offset = msg_offset;

	    page_pool->msgRef (page_list->first);
	} break;
	default:
	  // No-op
	    ;
    }
}

void
VideoStream::FrameSaver::processVideoFrame (VideoMessageInfo       * const mt_nonnull msg_info,
					    PagePool               * const mt_nonnull page_pool,
					    PagePool::PageListHead * const mt_nonnull page_list,
					    Size                     const msg_len,
					    Size                     const msg_offset)
{
    logD (frame_saver, _func, "0x", fmt_hex, (UintPtr) this);

    switch (msg_info->frame_type) {
	case VideoFrameType::KeyFrame: {
	    logD (frame_saver, _func, msg_info->frame_type);

	    if (got_saved_keyframe)
		saved_keyframe.page_pool->msgUnref (saved_keyframe.page_list.first);

	    got_saved_keyframe = true;
	    saved_keyframe.msg_info = *msg_info;
	    saved_keyframe.page_pool = page_pool;
	    saved_keyframe.page_list = *page_list;
	    saved_keyframe.msg_len = msg_len;
	    saved_keyframe.msg_offset = msg_offset;

	    page_pool->msgRef (page_list->first);
	} break;
	case VideoFrameType::AvcSequenceHeader: {
	    logD (frame_saver, _func, msg_info->frame_type);

	    if (got_saved_avc_seq_hdr)
		saved_avc_seq_hdr.page_pool->msgUnref (saved_avc_seq_hdr.page_list.first);

	    got_saved_avc_seq_hdr = true;
	    saved_avc_seq_hdr.msg_info = *msg_info;
	    saved_avc_seq_hdr.page_pool = page_pool;
	    saved_avc_seq_hdr.page_list = *page_list;
	    saved_avc_seq_hdr.msg_len = msg_len;
	    saved_avc_seq_hdr.msg_offset = msg_offset;

	    page_pool->msgRef (page_list->first);
	} break;
	case VideoFrameType::AvcEndOfSequence: {
	    logD (frame_saver, _func, msg_info->frame_type);

	    if (got_saved_avc_seq_hdr)
		saved_avc_seq_hdr.page_pool->msgUnref (saved_avc_seq_hdr.page_list.first);

	    got_saved_avc_seq_hdr = false;
	} break;
	case VideoFrameType::RtmpSetMetaData: {
	    logD (frame_saver, _func, msg_info->frame_type);

	    if (got_saved_metadata)
		saved_metadata.page_pool->msgUnref (saved_metadata.page_list.first);

	    got_saved_metadata = true;
	    saved_metadata.msg_info = *msg_info;
	    saved_metadata.page_pool = page_pool;
	    saved_metadata.page_list = *page_list;
	    saved_metadata.msg_len = msg_len;
	    saved_metadata.msg_offset = msg_offset;

	    page_pool->msgRef (page_list->first);
	} break;
	case VideoFrameType::RtmpClearMetaData: {
	    logD (frame_saver, _func, msg_info->frame_type);

	    if (got_saved_metadata)
		saved_metadata.page_pool->msgUnref (saved_metadata.page_list.first);

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
	saved_keyframe.page_pool->msgUnref (saved_keyframe.page_list.first);

    if (got_saved_metadata)
	saved_metadata.page_pool->msgUnref (saved_metadata.page_list.first);

    if (got_saved_aac_seq_hdr)
	saved_aac_seq_hdr.page_pool->msgUnref (saved_aac_seq_hdr.page_list.first);

    if (got_saved_avc_seq_hdr)
	saved_avc_seq_hdr.page_pool->msgUnref (saved_avc_seq_hdr.page_list.first);
}

namespace {
    struct InformAudioMessage_Data {
	VideoStream::AudioMessageInfo *msg_info;
	PagePool                      *page_pool;
	PagePool::PageListHead        *page_list;
	Size                           msg_len;
	Size                           msg_offset;

	InformAudioMessage_Data (VideoStream::AudioMessageInfo * const msg_info,
				 PagePool                      * const page_pool,
				 PagePool::PageListHead        * const page_list,
				 Size                            const msg_len,
				 Size                            const msg_offset)
	    : msg_info (msg_info),
	      page_pool (page_pool),
	      page_list (page_list),
	      msg_len (msg_len),
	      msg_offset (msg_offset)
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
    event_handler->audioMessage (inform_data->msg_info,
				 inform_data->page_pool,
				 inform_data->page_list,
				 inform_data->msg_len,
				 inform_data->msg_offset,
				 cb_data);
}

namespace {
    struct InformVideoMessage_Data {
	VideoStream::VideoMessageInfo *msg_info;
	PagePool                      *page_pool;
	PagePool::PageListHead        *page_list;
	Size                           msg_len;
	Size                           msg_offset;

	InformVideoMessage_Data (VideoStream::VideoMessageInfo * const msg_info,
				 PagePool                      * const page_pool,
				 PagePool::PageListHead        * const page_list,
				 Size                            const msg_len,
				 Size                            const msg_offset)
	    : msg_info (msg_info),
	      page_pool (page_pool),
	      page_list (page_list),
	      msg_len (msg_len),
	      msg_offset (msg_offset)
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
    event_handler->videoMessage (inform_data->msg_info,
				 inform_data->page_pool,
				 inform_data->page_list,
				 inform_data->msg_len,
				 inform_data->msg_offset,
				 cb_data);
}

namespace {
    struct InformRtmpCommandMessage_Data {
	RtmpConnection           *conn;
	VideoStream::MessageInfo *msg_info;
	ConstMemory        const &method_name;
	AmfDecoder               *amf_decoder;

	InformRtmpCommandMessage_Data (RtmpConnection           * const  conn,
				       VideoStream::MessageInfo * const  msg_info,
				       ConstMemory                const &method_name,
				       AmfDecoder               * const amf_decoder)
	    : conn (conn),
	      msg_info (msg_info),
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
    InformRtmpCommandMessage_Data * const inform_data = static_cast <InformRtmpCommandMessage_Data*> (_inform_data);
    // TODO Save/restore amf_decoder state between  callback invocations.
    //      Viable option - abstract away the parsing process.
    event_handler->rtmpCommandMessage (inform_data->conn,
				       inform_data->msg_info,
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
VideoStream::fireAudioMessage (AudioMessageInfo       * const mt_nonnull msg_info,
			       PagePool               * const mt_nonnull page_pool,
			       PagePool::PageListHead * const mt_nonnull page_list,
			       Size                     const msg_len,
			       Size                     const msg_offset)
{
//    logD_ (_func, "timestamp: 0x", fmt_hex, (UintPtr) msg_info->timestamp);

    mutex.lock ();

    frame_saver.processAudioFrame (msg_info, page_pool, page_list, msg_len, msg_offset);

    InformAudioMessage_Data inform_data (msg_info, page_pool, page_list, msg_len, msg_offset);
    event_informer.informAll_unlocked (informAudioMessage, &inform_data);

    mutex.unlock ();
}

void
VideoStream::fireVideoMessage (VideoMessageInfo       * const mt_nonnull msg_info,
			       PagePool               * const mt_nonnull page_pool,
			       PagePool::PageListHead * const mt_nonnull page_list,
			       Size                     const msg_len,
			       Size                     const msg_offset)
{
//    logD_ (_func, "timestamp: 0x", fmt_hex, (UintPtr) msg_info->timestamp);

    mutex.lock ();

    frame_saver.processVideoFrame (msg_info, page_pool, page_list, msg_len, msg_offset);

    InformVideoMessage_Data inform_data (msg_info, page_pool, page_list, msg_len, msg_offset);
    event_informer.informAll_unlocked (informVideoMessage, &inform_data);

    mutex.unlock ();
}

void
VideoStream::fireRtmpCommandMessage (RtmpConnection    * const  mt_nonnull conn,
				     MessageInfo       * const  mt_nonnull msg_info,
				     ConstMemory         const &method_name,
				     AmfDecoder        * const  mt_nonnull amf_decoder)
{
    InformRtmpCommandMessage_Data inform_data (conn, msg_info, method_name, amf_decoder);
    event_informer.informAll (informVideoMessage, &inform_data);
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

