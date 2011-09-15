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


#ifndef __MOMENT__AV_RECORDER__H__
#define __MOMENT__AV_RECORDER__H__


#include <libmary/libmary.h>

#include <moment/storage.h>
#include <moment/av_muxer.h>


namespace Moment {

using namespace M;

class AvRecorder : public DependentCodeReferenced
{
public:
    struct Frontend {
	void (*error) (Exception *exc_,
		       void      *cb_data);
    };

private:
    // Tickets help to distinguish asynchronous messages from different streams.
    // The allows to ignore messages from the old stream when switching streams.
    class StreamTicket : public Referenced
    {
    public:
	AvRecorder * const av_recorder;

	StreamTicket (AvRecorder * const av_recorder)
	    : av_recorder (av_recorder)
	{
	}
    };

    class Recording : public Object
    {
    public:
	WeakCodeRef weak_av_recorder;
	AvRecorder *unsafe_av_recorder;

	Connection *conn;
	Storage::FileKey file_key;

	DeferredConnectionSender sender;

	Recording ()
	    : sender (this /* coderef_container */)
	{
	}
    };

    Ref<StreamTicket> cur_stream_ticket;

    mt_const ServerThreadContext *thread_ctx;
    mt_const Storage *storage;

    // Muxer operations should be synchronized with 'mutex'.
    mt_const AvMuxer *muxer;

    mt_mutex (mutex) Ref<Recording> recording;
    mt_mutex (mutex) bool paused;

    Mutex mutex;

    mt_const Cb<Frontend> frontend;

    mt_mutex (mutex) void doStop ();

    mt_iface (Sender::Frontend)
    mt_begin
      static Sender::Frontend sender_frontend;

      static void senderSendStateChanged (Sender::SendState  send_state,
					  void              *_self);

      static void senderClosed (Exception *exc_,
				void      *_self);
    mt_end

    mt_iface (VideoStream::EventHandler)
    mt_begin
      static VideoStream::EventHandler stream_handler;

      static void streamAudioMessage (VideoStream::AudioMessage * mt_nonnull msg,
				      void *_stream_ticket);

      static void streamVideoMessage (VideoStream::VideoMessage * mt_nonnull msg,
				      void *_stream_ticket);

      static void streamClosed (void *_stream_ticket);
    mt_end

public:
    void start (ConstMemory filename);

    void pause ();

    void resume ();

    void stop ();

    void setVideoStream (VideoStream *stream);

    mt_const void setMuxer (AvMuxer *muxer);

    mt_const void setFrontend (CbDesc<Frontend> const &frontend)
    {
	this->frontend = frontend;
    }

    mt_const void init (ServerThreadContext *thread_ctx,
			Storage             *storage);

    AvRecorder (Object *coderef_container);

    ~AvRecorder ();
};

}


#endif /* __MOMENT__AV_RECORDER__H__ */

