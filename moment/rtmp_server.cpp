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


#include <moment/amf_encoder.h>
#include <moment/amf_decoder.h>

#include <moment/rtmp_server.h>


using namespace M;

namespace Moment {

namespace {
LogGroup libMary_logGroup_rtmp_server ("rtmp_server", LogLevel::I);
}

Result
RtmpServer::doConnect (RtmpConnection::MessageInfo * const mt_nonnull msg_info,
		       AmfDecoder * const mt_nonnull decoder)
{
    double transaction_id;
    if (!decoder->decodeNumber (&transaction_id)) {
	logE_ (_func, "could not decode transaction_id");
	return Result::Failure;
    }

#if 0
    if (!decoder->skipObject ()) {
	logE_ (_func, "could not skip command object");
	return Result::Failure;
    }
#endif

    if (!decoder->beginObject ()) {
	logE_ (_func, "no command object");
	return Result::Failure;
    }

    Byte app_name_buf [1024];
    Size app_name_len;
    for (;;) {
	Byte field_name_buf [512];
	Size field_name_len;
	Size field_name_full_len;
	if (!decoder->decodeFieldName (Memory::forObject (field_name_buf), &field_name_len, &field_name_full_len)) {
	    logE_ (_func, "no \"app\" field in the command object");
	    return Result::Failure;
	}

	if (equal (ConstMemory (field_name_buf, field_name_len), "app")) {
	    Size app_name_full_len;
	    if (!decoder->decodeString (Memory::forObject (app_name_buf), &app_name_len, &app_name_full_len)) {
		logE_ (_func, "could not decode app name");
		return Result::Failure;
	    }
	    if (app_name_full_len > app_name_len) {
		logW_ (_func, "app name length exceeds limit "
		       "(length ", app_name_full_len, " bytes, limit ", sizeof (app_name_buf), " bytes)");
	    }
	    break;
	}

	if (!decoder->skipValue ()) {
	    logE_ (_func, "could not skip field value");
	    return Result::Failure;
	}
    }

    rtmp_conn->sendWindowAckSize (rtmp_conn->getLocalWackSize());
    rtmp_conn->sendSetPeerBandwidth (rtmp_conn->getRemoteWackSize(), 2 /* dynamic limit */);
    rtmp_conn->sendUserControl_StreamBegin (0 /* msg_stream_id */);

    {
	AmfAtom atoms [19];
	AmfEncoder encoder (atoms);

	encoder.addString ("_result");
	encoder.addNumber (1.0); // FIXME Incorrect transaction_id?
	encoder.addNullObject ();

	{
	    encoder.beginObject ();

	    encoder.addFieldName ("level");
	    encoder.addString ("status");

	    encoder.addFieldName ("code");
	    encoder.addString ("NetConnection.Connect.Success");

	    encoder.addFieldName ("description");
	    encoder.addString ("Connection succeeded.");

	    encoder.endObject ();
	}

	{
	    encoder.beginObject ();

	    encoder.addFieldName ("fmsVer");
	    encoder.addString ("MMNT/0,1,0,0");

	    encoder.addFieldName ("capabilities");
	    // TODO Define capabilities. Docs?
	    encoder.addNumber (31.0);

	    encoder.addFieldName ("mode");
	    encoder.addNumber (1.0);

	    encoder.endObject ();
	}

	Byte msg_buf [1024];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "encode() failed");
	    return Result::Failure;
	}

	rtmp_conn->sendCommandMessage_AMF0 (msg_info->msg_stream_id, ConstMemory (msg_buf, msg_len));

#if 0
	logD (proto_out, _func, "msg_len: ", msg_len);
	if (logLevelOn (proto_out, LogLevel::Debug))
	    hexdump (ConstMemory (msg_buf, msg_len));
#endif
    }

    if (frontend->connect) {
	Result res;
	if (!frontend.call_ret<Result> (&res, frontend->connect, /*(*/ ConstMemory (app_name_buf, app_name_len) /*)*/)) {
	    logE_ (_func, "frontend gone");
	    return Result::Failure;
	}

	if (!res) {
	    logE_ (_func, "frontend->connect() failed");
	    return Result::Failure;
	}
    }

    return Result::Success;
}

Result
RtmpServer::doPlay (RtmpConnection::MessageInfo * const mt_nonnull msg_info,
		    AmfDecoder * const mt_nonnull decoder)
{
    logD (rtmp_server, _func_);

    if (playing.get()) {
	logW_ (_func, "already playing");
	return Result::Success;
    }
    playing.set (1);

    double transaction_id;
    if (!decoder->decodeNumber (&transaction_id)) {
	logE_ (_func, "could not decode transaction_id");
	return Result::Failure;
    }

    if (!decoder->skipObject ()) {
	logE_ (_func, "could not skip command object");
	return Result::Failure;
    }

    Byte vs_name_buf [512];
    Size vs_name_len;
    Size vs_name_full_len;
    if (!decoder->decodeString (Memory::forObject (vs_name_buf), &vs_name_len, &vs_name_full_len)) {
	logE_ (_func, "could not decode video stream name");
	return Result::Failure;
    }
    if (vs_name_full_len > vs_name_len) {
	logW_ (_func, "video stream name length exceeds limit "
	       "(length ", vs_name_full_len, " bytes, limit ", sizeof (vs_name_buf), " bytes)");
    }

    {
	AmfAtom atoms [4];
	AmfEncoder encoder (atoms);

	encoder.addString ("_result");
	encoder.addNumber (transaction_id);
	encoder.addNullObject ();
	encoder.addNullObject ();

	Byte msg_buf [512];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "could not encode reply");
	    return Result::Failure;
	}

	rtmp_conn->sendCommandMessage_AMF0 (msg_info->msg_stream_id, ConstMemory (msg_buf, msg_len));
    }

    {
      // Sending onStatus reply "Reset".

	AmfAtom atoms [15];
	AmfEncoder encoder (atoms);

	encoder.addString ("onStatus");
	encoder.addNumber (0.0 /* transaction_id */);
	encoder.addNullObject ();

	encoder.beginObject ();

	encoder.addFieldName ("level");
	encoder.addString ("status");

	encoder.addFieldName ("code");
	encoder.addString ("NetStream.Play.Reset");

	Ref<String> description_str = makeString ("Playing and resetting ", ConstMemory (vs_name_buf, vs_name_len), ".");
	encoder.addFieldName ("description");
	encoder.addString (description_str->mem());

	encoder.addFieldName ("details");
	encoder.addString (ConstMemory (vs_name_buf, vs_name_len));

	encoder.addFieldName ("clientid");
	encoder.addNumber (1.0);

	encoder.endObject ();

	Byte msg_buf [4096];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "could not encode onStatus message");
	    return Result::Failure;
	}

	rtmp_conn->sendCommandMessage_AMF0 (msg_info->msg_stream_id, ConstMemory (msg_buf, msg_len));
    }

    // TODO 

//    if (!playing) {
	// TODO send StreamIsRecorded
//        rtmp_conn->sendUserControl_StreamIsRecorded (msg_info->msg_stream_id);
	rtmp_conn->sendUserControl_StreamBegin (msg_info->msg_stream_id);
//    }

    {
      // Sending onStatus reply "Start".

	AmfAtom atoms [15];
	AmfEncoder encoder (atoms);

	encoder.addString ("onStatus");
	encoder.addNumber (0.0 /* transaction_id */);
	encoder.addNullObject ();

	encoder.beginObject ();

	encoder.addFieldName ("level");
	encoder.addString ("status");

	encoder.addFieldName ("code");
	encoder.addString ("NetStream.Play.Start");

	Ref<String> description_str = makeString ("Started playing ", ConstMemory (vs_name_buf, vs_name_len), ".");
	encoder.addFieldName ("description");
	encoder.addString (description_str->mem());

	encoder.addFieldName ("details");
	encoder.addString (ConstMemory (vs_name_buf, vs_name_len));

	encoder.addFieldName ("clientid");
	encoder.addNumber (1.0);

	encoder.endObject ();

	Byte msg_buf [4096];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "could not encode onStatus message");
	    return Result::Failure;
	}

	rtmp_conn->sendCommandMessage_AMF0 (msg_info->msg_stream_id, ConstMemory (msg_buf, msg_len));
    }

    {
	Result res;
	if (!frontend.call_ret<Result> (&res, frontend->startWatching, /*(*/ ConstMemory (vs_name_buf, vs_name_len) /*)*/)) {
	    logE_ (_func, "frontend gone");
	    return Result::Failure;
	}

	if (!res) {
	    logE_ (_func, "frontend->startWatching() failed");
	    return Result::Failure;
	}
    }

    return Result::Success;
}

Result
RtmpServer::doPublish (RtmpConnection::MessageInfo * const mt_nonnull msg_info,
		       AmfDecoder * const mt_nonnull decoder)
{
    logD (rtmp_server, _func_);

    // XXX Ugly
    if (playing.get()) {
	logW_ (_func, "already playing");
	return Result::Success;
    }
    playing.set (1);

    double transaction_id;
    if (!decoder->decodeNumber (&transaction_id)) {
	logE_ (_func, "could not decode transaction_id");
	return Result::Failure;
    }

    if (!decoder->skipObject ()) {
	logE_ (_func, "could not skip command object");
	return Result::Failure;
    }

    Byte vs_name_buf [512];
    Size vs_name_len;
    Size vs_name_full_len;
    if (!decoder->decodeString (Memory::forObject (vs_name_buf), &vs_name_len, &vs_name_full_len)) {
	logE_ (_func, "could not decode video stream name");
	return Result::Failure;
    }
    if (vs_name_full_len > vs_name_len) {
	logW_ (_func, "video stream name length exceeds limit "
	       "(length ", vs_name_full_len, " bytes, limit ", sizeof (vs_name_buf), " bytes)");
    }

    if (frontend && frontend->startStreaming) {
	Result res;
	if (!frontend.call_ret<Result> (&res, frontend->startStreaming, /*(*/ ConstMemory (vs_name_buf, vs_name_len) /*)*/)) {
	    logE_ (_func, "frontend gone");
	    return Result::Failure;
	}

	if (!res) {
	    logE_ (_func, "frontend->startStreaming() failed");
	    return Result::Failure;
	}
    }

    // TODO Subscribe for translation stop? (probably not here)

    rtmp_conn->sendUserControl_StreamBegin (msg_info->msg_stream_id);

    {
      // TODO sendSimpleResult

	AmfAtom atoms [4];
	AmfEncoder encoder (atoms);

	encoder.addString ("_result");
	encoder.addNumber (transaction_id);
	encoder.addNullObject ();
	encoder.addNullObject ();

	Byte msg_buf [512];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "could not encode reply");
	    return Result::Failure;
	}

	rtmp_conn->sendCommandMessage_AMF0 (msg_info->msg_stream_id, ConstMemory (msg_buf, msg_len));
    }

#if 0
    {
      // Sending onStatus reply.

	AmfAtom atoms [5];
	AmfEncoder encoder (atoms);

	encoder.addString ("onStatus");
	encoder.addNumber (0.0 /* transaction_id */);
	encoder.addNullObject ();
	encoder.addString (ConstMemory (vs_name_buf, vs_name_len));
	encoder.addString ("live");

	Byte msg_buf [512];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "could not encode onStatus message");
	    return Result::Failure;
	}

	rtmp_conn->sendCommandMessage_AMF0 (msg_info->msg_stream_id, ConstMemory (msg_buf, msg_len));
    }
#endif

    {
      // Sending onStatus reply "Reset".

	AmfAtom atoms [15];
	AmfEncoder encoder (atoms);

	encoder.addString ("onStatus");
	encoder.addNumber (0.0 /* transaction_id */);
	encoder.addNullObject ();

	encoder.beginObject ();

	encoder.addFieldName ("level");
	encoder.addString ("status");

	encoder.addFieldName ("code");
	encoder.addString ("NetStream.Play.Reset");

	Ref<String> description_str = makeString ("Playing and resetting ", ConstMemory (vs_name_buf, vs_name_len), ".");
	encoder.addFieldName ("description");
	encoder.addString (description_str->mem());

	encoder.addFieldName ("details");
	encoder.addString (ConstMemory (vs_name_buf, vs_name_len));

	encoder.addFieldName ("clientid");
	encoder.addNumber (1.0);

	encoder.endObject ();

	Byte msg_buf [4096];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "could not encode onStatus message");
	    return Result::Failure;
	}

	rtmp_conn->sendCommandMessage_AMF0 (msg_info->msg_stream_id, ConstMemory (msg_buf, msg_len));
    }

    {
      // Sending onStatus reply "Start".

	AmfAtom atoms [15];
	AmfEncoder encoder (atoms);

	encoder.addString ("onStatus");
	encoder.addNumber (0.0 /* transaction_id */);
	encoder.addNullObject ();

	encoder.beginObject ();

	encoder.addFieldName ("level");
	encoder.addString ("status");

	encoder.addFieldName ("code");
	encoder.addString ("NetStream.Play.Start");

	Ref<String> description_str = makeString ("Started playing ", ConstMemory (vs_name_buf, vs_name_len), ".");
	encoder.addFieldName ("description");
	encoder.addString (description_str->mem());

	encoder.addFieldName ("details");
	encoder.addString (ConstMemory (vs_name_buf, vs_name_len));

	encoder.addFieldName ("clientid");
	encoder.addNumber (1.0);

	encoder.endObject ();

	Byte msg_buf [4096];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "could not encode onStatus message");
	    return Result::Failure;
	}

	rtmp_conn->sendCommandMessage_AMF0 (msg_info->msg_stream_id, ConstMemory (msg_buf, msg_len));
    }

    return Result::Success;
}

void
RtmpServer::sendVideoMessage (VideoStream::VideoMessageInfo * const mt_nonnull msg_info,
			      PagePool::PageListHead        * const mt_nonnull page_list,
			      Size                            const msg_len,
			      Size                            const msg_offset)
{
//    logD (rtmp_server, _func);

    // TODO Do not ignore RtmpClearMetaData.
    if (msg_info->frame_type == VideoStream::VideoFrameType::RtmpClearMetaData)
	return;

    RtmpConnection::MessageDesc mdesc;
    mdesc.timestamp = msg_info->timestamp;
//    logD_ (_func, "timestamp: 0x", fmt_hex, msg_info->timestamp);

    RtmpConnection::ChunkStream *chunk_stream;
    if (msg_info->frame_type == VideoStream::VideoFrameType::RtmpSetMetaData) {
	mdesc.timestamp = 0;
	mdesc.msg_type_id = RtmpConnection::RtmpMessageType::Data_AMF0;
	chunk_stream = rtmp_conn->data_chunk_stream;
    } else {
	mdesc.msg_type_id = RtmpConnection::RtmpMessageType::VideoMessage;
	chunk_stream = video_chunk_stream;
    }

    mdesc.msg_stream_id = RtmpConnection::DefaultMessageStreamId;
    mdesc.msg_len = msg_len;
    mdesc.cs_hdr_comp = true;

    rtmp_conn->sendMessagePages (&mdesc, chunk_stream, page_list, msg_offset, msg_info->prechunk_size);
}

void
RtmpServer::sendAudioMessage (VideoStream::AudioMessageInfo * const mt_nonnull msg_info,
			      PagePool::PageListHead        * const mt_nonnull page_list,
			      Size                            const msg_len,
			      Size                            const msg_offset)
{
//    logD (rtmp_server, _func);

    RtmpConnection::MessageDesc mdesc;
    mdesc.timestamp = msg_info->timestamp;
    mdesc.msg_type_id = RtmpConnection::RtmpMessageType::AudioMessage;
    mdesc.msg_stream_id = RtmpConnection::DefaultMessageStreamId;
    mdesc.msg_len = msg_len;
    mdesc.cs_hdr_comp = true;

    rtmp_conn->sendMessagePages (&mdesc, audio_chunk_stream, page_list, msg_offset, msg_info->prechunk_size);
}

void
RtmpServer::sendInitialMessages_unlocked (VideoStream::FrameSaver * const mt_nonnull frame_saver)
{
    VideoStream::SavedFrame saved_frame;
    VideoStream::SavedAudioFrame saved_audio_frame;

    if (frame_saver->getSavedMetaData (&saved_frame)) {
	saved_frame.msg_info.timestamp = 0;
	sendVideoMessage (&saved_frame.msg_info,
			  &saved_frame.page_list,
			  saved_frame.msg_len,
			  saved_frame.msg_offset);
    }

    if (frame_saver->getSavedAacSeqHdr (&saved_audio_frame)) {
	saved_audio_frame.msg_info.timestamp = 0;
	sendAudioMessage (&saved_audio_frame.msg_info,
			  &saved_audio_frame.page_list,
			  saved_audio_frame.msg_len,
			  saved_audio_frame.msg_offset);
    }

    if (frame_saver->getSavedAvcSeqHdr (&saved_frame)) {
	saved_frame.msg_info.timestamp = 0;
	sendVideoMessage (&saved_frame.msg_info,
			  &saved_frame.page_list,
			  saved_frame.msg_len,
			  saved_frame.msg_offset);
    }

    if (frame_saver->getSavedKeyframe (&saved_frame)) {
	saved_frame.msg_info.timestamp = 0;
	sendVideoMessage (&saved_frame.msg_info,
			  &saved_frame.page_list,
			  saved_frame.msg_len,
			  saved_frame.msg_offset);
    }
}

Result
RtmpServer::commandMessage (RtmpConnection::MessageInfo * const mt_nonnull msg_info,
			    PagePool               * const page_pool,
			    PagePool::PageListHead * const page_list,
			    Size                     const msg_len,
			    AmfEncoding              const /* amf_encoding */)
{
    logD (rtmp_server, _func_);

    if (msg_len == 0)
	return Result::Success;

    PagePool::PageListArray pl_array (page_list->first, msg_len);
    AmfDecoder decoder (AmfEncoding::AMF0, &pl_array, msg_len);

    Byte method_name [256];
    Size method_name_len;
    if (!decoder.decodeString (Memory::forObject (method_name),
			       &method_name_len,
			       NULL /* ret_full_len */))
    {
	logE_ (_func, "could not decode method name");
	return Result::Failure;
    }

    logI (rtmp_server, _func, "method: ", ConstMemory (method_name, method_name_len));

    ConstMemory method_mem (method_name, method_name_len);
    if (!compare (method_mem, "connect")) {
//	decoder.decodeNumber ();
//	decoder.beginObject ();
	// TODO Decode URL

	return doConnect (msg_info, &decoder);
    } else
    if (!compare (method_mem, "createStream")) {
	return rtmp_conn->doCreateStream (msg_info, &decoder);
    } else
    if (!compare (method_mem, "FCPublish")) {
      // TEMPORAL TEST
	return rtmp_conn->doReleaseStream (msg_info, &decoder);
    } else
    if (!compare (method_mem, "releaseStream")) {
	return rtmp_conn->doReleaseStream (msg_info, &decoder);
    } else
    if (!compare (method_mem, "closeStream")) {
	return rtmp_conn->doCloseStream (msg_info, &decoder);
    } else
    if (!compare (method_mem, "deleteStream")) {
	return rtmp_conn->doDeleteStream (msg_info, &decoder);
    } else
    if (!compare (method_mem, "receiveVideo")) {
      // TODO
    } else
    if (!compare (method_mem, "receiveAudio")) {
      // TODO
    } else
    if (!compare (method_mem, "play") ||
	!compare (method_mem, "pause"))
    {
	return doPlay (msg_info, &decoder);
    } else
    if (!compare (method_mem, "publish")) {
	return doPublish (msg_info, &decoder);
    } else
    if (!compare (method_mem, "@setDataFrame")) {
#if 0
	logD_ (_func, method_mem);

	{
	    PagePool::Page * const page = page_list->first;
	    logD_ (_func, "page: 0x", fmt_hex, (UintPtr) page);
	    if (page)
		hexdump (logs, page->mem());
	}
#endif

	Size const msg_offset = decoder.getCurOffset ();
	assert (msg_offset <= msg_len);
//	logD_ (_func, "msg_offset: ", msg_offset);

	VideoStream::VideoMessageInfo video_msg_info;
	video_msg_info.timestamp = msg_info->timestamp;
	video_msg_info.prechunk_size = msg_info->prechunk_size;
	video_msg_info.frame_type = VideoStream::VideoFrameType::RtmpSetMetaData;
	video_msg_info.codec_id = VideoStream::VideoCodecId::Unknown;

	return rtmp_conn->fireVideoMessage (&video_msg_info, page_pool, page_list, msg_len - msg_offset, msg_offset);
    } else
    if (!compare (method_mem, "@clearDataFrame")) {
#if 0
	logD_ (_func, method_mem);

	{
	    PagePool::Page * const page = page_list->first;
	    if (page)
		hexdump (logs, page->mem());
	}
#endif

	Size const msg_offset = decoder.getCurOffset ();
	assert (msg_offset <= msg_len);

	VideoStream::VideoMessageInfo video_msg_info;
	video_msg_info.timestamp = msg_info->timestamp;
	video_msg_info.prechunk_size = msg_info->prechunk_size;
	video_msg_info.frame_type = VideoStream::VideoFrameType::RtmpClearMetaData;
	video_msg_info.codec_id = VideoStream::VideoCodecId::Unknown;

	return rtmp_conn->fireVideoMessage (&video_msg_info, page_pool, page_list, msg_len - msg_offset, msg_offset);
    } else {
	if (frontend && frontend->commandMessage) {
	    CommandResult res;
	    if (!frontend.call_ret<CommandResult> (&res, frontend->commandMessage, /*(*/
			 rtmp_conn, method_mem, msg_info, &decoder /*)*/))
	    {
		logE_ (_func, "frontend gone");
		return Result::Failure;
	    }

	    if (res == CommandResult::Failure) {
		logE_ (_func, "frontend->cmmandMessage() failed (", method_mem, ")");
		return Result::Failure;
	    }

	    if (res == CommandResult::UnknownCommand)
		logW_ (_func, "unknown method: ", method_mem);
	    else
		assert (res == CommandResult::Success);
	} else {
	    logW_ (_func, "unknown method: ", method_mem);
	}
    }

    return Result::Success;
}

Result
RtmpServer::encodeMetaData (MetaData * const mt_nonnull metadata,
			    PagePool * const mt_nonnull page_pool,
			    PagePool::PageListHead * const mt_nonnull page_list,
			    Size     * const ret_msg_len,
			    VideoStream::VideoMessageInfo * const ret_msg_info)
{
    AmfAtom atoms [128];
    AmfEncoder encoder (atoms);

    encoder.addString ("onMetaData");
// Unnecessary    encoder.addNumber (1.0);

    encoder.addEcmaArray (0 /* num_entries */);
    AmfAtom * const toplevel_array_atom = encoder.getLastAtom ();
    Uint32 num_entries = 0;

    if (metadata->got_flags & MetaData::VideoCodecId) {
	encoder.addFieldName ("videocodecid");
	encoder.addNumber (metadata->video_codec_id);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::AudioCodecId) {
	encoder.addFieldName ("audiocodecid");
	encoder.addNumber (metadata->audio_codec_id);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::VideoWidth) {
	encoder.addFieldName ("width");
	encoder.addNumber (metadata->video_width);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::VideoHeight) {
	encoder.addFieldName ("height");
	encoder.addNumber (metadata->video_height);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::AspectRatioX) {
	encoder.addFieldName ("AspectRatioX");
	encoder.addNumber (metadata->aspect_ratio_x);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::AspectRatioY) {
	encoder.addFieldName ("AspectRatioY");
	encoder.addNumber (metadata->aspect_ratio_y);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::VideoDataRate) {
	encoder.addFieldName ("videodatarate");
	encoder.addNumber (metadata->video_data_rate);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::AudioDataRate) {
	encoder.addFieldName ("audiodatarate");
	encoder.addNumber (metadata->audio_data_rate);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::VideoFrameRate) {
	encoder.addFieldName ("framerate");
	encoder.addNumber (metadata->video_frame_rate);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::AudioSampleRate) {
	encoder.addFieldName ("audiosamplerate");
	encoder.addNumber (metadata->audio_sample_rate);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::AudioSampleSize) {
	encoder.addFieldName ("audiosamplesize");
	encoder.addNumber (metadata->audio_sample_size);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::NumChannels) {
	encoder.addFieldName ("stereo");
	if (metadata->num_channels >= 2) {
	    encoder.addBoolean (true);
	} else {
	    encoder.addBoolean (false);
	}
	++num_entries;
    }

    encoder.endObject ();

    toplevel_array_atom->setEcmaArraySize (num_entries);

    Byte msg_buf [4096];
    Size msg_len;
    if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	logE_ (_func, "encode() failed");
	return Result::Failure;
    }

    {
	RtmpConnection::PrechunkContext prechunk_ctx;
	RtmpConnection::fillPrechunkedPages (&prechunk_ctx,
					     ConstMemory (msg_buf, msg_len),
					     page_pool,
					     page_list,
					     RtmpConnection::DefaultDataChunkStreamId,
					     0 /* timestamp */,
					     true /* first_chunk */);
    }

    if (ret_msg_len)
	*ret_msg_len = msg_len;

    if (ret_msg_info) {
	ret_msg_info->timestamp = 0;
	ret_msg_info->prechunk_size = RtmpConnection::PrechunkSize;
	ret_msg_info->frame_type = VideoStream::VideoFrameType::RtmpSetMetaData;
	ret_msg_info->codec_id = VideoStream::VideoCodecId::Unknown;
    }

    return Result::Success;
}

}

