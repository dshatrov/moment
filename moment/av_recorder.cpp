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


#include <moment/av_recorder.h>


using namespace M;

namespace Moment {

namespace {
LogGroup libMary_logGroup_recorder ("av_recorder", LogLevel::I);
}

Sender::Frontend AvRecorder::sender_frontend {
    senderSendStateChanged,
    senderClosed
};

mt_mutex (mutex) void
AvRecorder::doStop ()
{
    cur_stream_ticket = NULL;

    if (recording) {
	if (!muxer->endMuxing ())
	    logE (recorder, _func, "muxer->endMuxing() failed: ", exc->toString());

	storage->releaseFile (recording->file_key);

	recording = NULL;
    }
}

void
AvRecorder::senderSendStateChanged (Sender::SendState   const send_state,
				    void              * const _self)
{
  // TODO
}

void
AvRecorder::senderClosed (Exception * const exc_,
			  void      * const _self)
{
  // TODO
}

VideoStream::EventHandler AvRecorder::stream_handler = {
    streamAudioMessage,
    streamVideoMessage,
    NULL /* rtmpCommandMessage */,
    streamClosed
};

void
AvRecorder::streamAudioMessage (VideoStream::AudioMessage * const mt_nonnull msg,
				void * const _stream_ticket)
{
    StreamTicket * const stream_ticket = static_cast <StreamTicket*> (_stream_ticket);
    AvRecorder * const self = stream_ticket->av_recorder;

    self->mutex.lock ();
    if (self->cur_stream_ticket != stream_ticket) {
	self->mutex.unlock ();
	return;
    }

    if (self->recording) {
	if (!self->muxer->muxAudioMessage (msg)) {
	    logE (recorder, _func, "muxer->muxAudioMessage() failed: ", exc->toString());
	    self->doStop ();
	}
    }

    self->mutex.unlock ();
}

void
AvRecorder::streamVideoMessage (VideoStream::VideoMessage * const mt_nonnull msg,
				void * const _stream_ticket)
{
    StreamTicket * const stream_ticket = static_cast <StreamTicket*> (_stream_ticket);
    AvRecorder * const self = stream_ticket->av_recorder;

    self->mutex.lock ();
    if (self->cur_stream_ticket != stream_ticket) {
	self->mutex.unlock ();
	return;
    }

    if (self->recording) {
	if (!self->muxer->muxVideoMessage (msg)) {
	    logE (recorder, _func, "muxer->muxVideoMessage() failed: ", exc->toString());
	    self->doStop ();
	}
    }

    self->mutex.unlock ();
}

void
AvRecorder::streamClosed (void * const _stream_ticket)
{
    StreamTicket * const stream_ticket = static_cast <StreamTicket*> (_stream_ticket);
    AvRecorder * const self = stream_ticket->av_recorder;

    self->mutex.lock ();
    if (self->cur_stream_ticket != stream_ticket) {
	self->mutex.unlock ();
	return;
    }

    self->doStop ();
    self->mutex.unlock ();
}

void
AvRecorder::start (ConstMemory const filename)
{
    mutex.lock ();

    if (recording) {
	logW (recorder, _func, "Already recording");
	mutex.unlock ();
	return;
    }

    recording = grab (new Recording);

    recording->file_key = storage->openFile (filename, &recording->conn);
    if (!recording->file_key) {
	logE (recorder, _func, "storage->openFile() failed for filename ",
	      filename, ": ", exc->toString());
	recording = NULL;
	mutex.unlock ();
	return;
    }

    recording->weak_av_recorder = this;
    recording->unsafe_av_recorder = this;

    recording->sender.setConnection (recording->conn);
    recording->sender.setQueue (thread_ctx->getDeferredConnectionSenderQueue());
    recording->sender.setFrontend (
	    CbDesc<Sender::Frontend> (&sender_frontend,
				      recording /* cb_data */,
				      recording /* coderef_container */));

    paused = false;

    mutex.unlock ();
}

void
AvRecorder::pause ()
{
    mutex.lock ();
    paused = true;
    // TODO Track keyframes.
    mutex.unlock ();
}

void
AvRecorder::resume ()
{
    mutex.lock ();
    paused = false;
    mutex.unlock ();
}

void
AvRecorder::stop ()
{
    mutex.lock ();
    doStop ();
    mutex.unlock ();
}

void
AvRecorder::setVideoStream (VideoStream * const stream)
{
    mutex.lock ();

    cur_stream_ticket = grab (new StreamTicket (this));

    stream->getEventInformer()->subscribe (
	    CbDesc<VideoStream::EventHandler> (
		    &stream_handler, cur_stream_ticket /* cb_data */, getCoderefContainer(), cur_stream_ticket));

    mutex.unlock ();
}

mt_const void
AvRecorder::setMuxer (AvMuxer * const muxer)
{
    this->muxer = muxer;
}

mt_const void
AvRecorder::init (ServerThreadContext * const thread_ctx,
		  Storage             * const storage)
{
    this->thread_ctx = thread_ctx;
    this->storage = storage;
}

AvRecorder::AvRecorder (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container),
      thread_ctx (NULL),
      storage (NULL),
      paused (false)
{
}

AvRecorder::~AvRecorder ()
{
    mutex.lock ();
    doStop ();
    mutex.unlock ();
}

}

