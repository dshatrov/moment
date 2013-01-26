/*  Moment Video Server - High performance media server
    Copyright (C) 2011-2013 Dmitry Shatrov
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


#ifndef MOMENT__CHANNEL__H__
#define MOMENT__CHANNEL__H__


#include <libmary/libmary.h>
#include <moment/channel_options.h>
#include <moment/media_source.h>
#include <moment/playback.h>
#include <moment/moment_server.h>


namespace Moment {

using namespace M;

class Channel : public Object
{
private:
    StateMutex mutex;


  // _____________________________ event_informer ______________________________

public:
    struct ChannelEvents
    {
        void (*startItem)      (void *cb_data);
        void (*stopItem)       (void *cb_data);
        void (*newVideoStream) (void *cb_data);
    };

private:
    Informer_<ChannelEvents> event_informer;

    static void informStartItem (ChannelEvents * const events,
                                 void          * const cb_data,
                                 void          * const /* inform_data */)
        { if (events->startItem) events->startItem (cb_data); }

    static void informStopItem (ChannelEvents * const events,
                                void          * const cb_data,
                                void          * const /* inform_data */)
        { if (events->stopItem) events->stopItem (cb_data); }

    static void informNewVideoStream (ChannelEvents * const events,
                                      void          * const cb_data,
                                      void          * const /* inform_data */)
        { if (events->newVideoStream) events->newVideoStream (cb_data); }

    void fireStartItem ()
        { event_informer.informAll (informStartItem, NULL); }
    void fireStopItem ()
        { event_informer.informAll (informStopItem, NULL); }
    void fireNewVideoStream ()
        { event_informer.informAll (informNewVideoStream, NULL); }

public:
    Informer_<ChannelEvents>* getEventInformer () { return &event_informer; }


  // ________________________________ playback _________________________________

private:
    Playback playback;

    mt_iface (Playback::Frontend)
    mt_begin
      static Playback::Frontend playback_frontend;

      static void startPlaybackItem (Playlist::Item          *item,
				     Time                     seek,
				     Playback::AdvanceTicket *advance_ticket,
				     void                    *_self);

      static void stopPlaybackItem (void *_self);
    mt_end

public:
    Playback* getPlayback () { return &playback; }

  // ___________________________________________________________________________


private:
    mt_const Ref<ChannelOptions> channel_opts;

    mt_const DataDepRef<MomentServer> moment;
    mt_const DataDepRef<Timers>       timers;
    mt_const DataDepRef<PagePool>     page_pool;

    DeferredProcessor::Task deferred_task;
    DeferredProcessor::Registration deferred_reg;

  // Stream source description

    mt_mutex (mutex) Ref<String> stream_spec;
    mt_mutex (mutex) bool is_chain;
    mt_mutex (mutex) bool force_transcode;
    mt_mutex (mutex) bool force_transcode_audio;
    mt_mutex (mutex) bool force_transcode_video;

  // Video stream state

    class StreamData : public Referenced
    {
    public:
        Channel * const channel;

	void * const stream_ticket;
	VirtRef const stream_ticket_ref;

	mt_mutex (GstStreamCtl::mutex) bool stream_closed;
        mt_mutex (GstStreamCtl::mutex) Count num_watchers;

	StreamData (Channel        * const channel,
		    void           * const stream_ticket,
		    VirtReferenced * const stream_ticket_ref)
	    : channel (channel),
	      stream_ticket (stream_ticket),
	      stream_ticket_ref (stream_ticket_ref),
	      stream_closed (false),
              num_watchers (0)
	{
	}
    };

    mt_mutex (mutex) Ref<MediaSource> media_source;
    // Serves as internal ticket.
    mt_mutex (mutex) Ref<StreamData> cur_stream_data;

    mt_mutex (mutex) Ref<VideoStream> video_stream;
    mt_mutex (mutex) GenericInformer::SubscriptionKey video_stream_events_sbn;
    mt_mutex (mutex) MomentServer::VideoStreamKey video_stream_key;

    mt_mutex (mutex) void *stream_ticket;
    mt_mutex (mutex) VirtRef stream_ticket_ref;

    mt_mutex (mutex) bool stream_stopped;
    mt_mutex (mutex) bool got_video;

    mt_mutex (mutex) Time stream_start_time;


  // ____________________________ connect on demand ____________________________

    mt_mutex (mutex) Timers::TimerKey connect_on_demand_timer;

    mt_iface (VideoStream::EventHandler)
        static VideoStream::EventHandler const stream_event_handler;

        static void numWatchersChanged (Count  num_watchers,
                                        void  *_data);
    mt_iface_end

    static void connectOnDemandTimerTick (void *_stream_data);

    void beginConnectOnDemand (bool start_timer);

  // ___________________________________________________________________________


    void setStreamParameters (VideoStream * mt_nonnull video_stream);

    mt_mutex (mutex) void createStream (Time initial_seek);

    static gpointer streamThreadFunc (gpointer _media_source);

    mt_mutex (mutex) void closeStream (bool replace_video_stream);

    mt_unlocks (mutex) void doRestartStream (bool from_ondemand_reconnect = false);

    mt_iface (MediaSource::Frontend)
    mt_begin
      static MediaSource::Frontend media_source_frontend;

      static void streamError (void *_stream_data);
      static void streamEos   (void *_stream_data);
      static void noVideo     (void *_stream_data);
      static void gotVideo    (void *_stream_data);
      static void streamStatusEvent (void *_stream_data);
    mt_end

    static bool deferredTask (void *_self);

public:
    void beginVideoStream (ConstMemory     stream_spec,
			   bool            is_chain,
                           bool            force_transcode,
                           bool            force_transcode_audio,
                           bool            force_transcode_video,
			   void           *stream_ticket,
			   VirtReferenced *stream_ticket_ref,
			   Time            seek = 0);

    void endVideoStream ();
    void restartStream  ();

    bool isSourceOnline ();


  // ______________________________ traffic stats ______________________________

private:
    mt_mutex (mutex) Uint64 rx_bytes_accum;
    mt_mutex (mutex) Uint64 rx_audio_bytes_accum;
    mt_mutex (mutex) Uint64 rx_video_bytes_accum;

public:
    class TrafficStats
    {
    public:
	Uint64 rx_bytes;
	Uint64 rx_audio_bytes;
	Uint64 rx_video_bytes;
	Time   time_elapsed;
    };

    void getTrafficStats (TrafficStats *ret_traffic_stats);

    void resetTrafficStats ();

  // ___________________________________________________________________________


public:
    mt_const void init (MomentServer   * mt_nonnull moment,
                        ChannelOptions * mt_nonnull channel_opts);

     Channel ();
    ~Channel ();
};

}


#endif /* MOMENT__CHANNEL__H__ */

