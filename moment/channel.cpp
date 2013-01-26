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


#include <moment/channel.h>


using namespace M;

namespace Moment {

static LogGroup libMary_logGroup_ctl ("moment.channel", LogLevel::D);

Playback::Frontend Channel::playback_frontend = {
    startPlaybackItem,
    stopPlaybackItem
};

void
Channel::startPlaybackItem (Playlist::Item          * const item,
			    Time                      const seek,
			    Playback::AdvanceTicket * const advance_ticket,
			    void                    * const _self)
{
    logD_ (_func_, ", seek: ", seek);

    Channel * const self = static_cast <Channel*> (_self);

    bool const got_chain_spec = item->chain_spec && !item->chain_spec.isNull();
    bool const got_uri = item->uri && !item->uri.isNull();

    logD_ (_self_func, "got_chain_spec: ", got_chain_spec, ", got_uri: ", got_uri);

    if (got_chain_spec && got_uri) {
	logW_ (_func, "Both chain spec and uri are specified for a playlist item. "
	       "Ignoring the uri.");
    }

    bool item_started = true;
    if (got_chain_spec) {
	self->beginVideoStream (item->chain_spec->mem(),
                                true /* is_chain */,
                                item->force_transcode,
                                item->force_transcode_audio,
                                item->force_transcode_video,
                                advance_ticket /* stream_ticket */,
                                advance_ticket /* stream_ticket_ref */,
                                seek);
    } else
    if (got_uri) {
	self->beginVideoStream (item->uri->mem(),
                                false /* is_chain */,
                                item->force_transcode,
                                item->force_transcode_audio,
                                item->force_transcode_video,
                                advance_ticket /* stream_ticket */,
                                advance_ticket /* stream_ticket_ref */,
                                seek);
    } else {
	logW_ (_func, "Nor chain spec, no uri is specified for a playlist item.");
	self->playback.advance (advance_ticket);
	item_started = false;
    }

    if (item_started) {
	logD_ (_func, "firing startItem");
	self->fireStartItem ();
    }
}

void
Channel::stopPlaybackItem (void * const _self)
{
    logD_ (_func_);

    Channel * const self = static_cast <Channel*> (_self);
    self->endVideoStream ();
    self->fireStopItem ();
}

VideoStream::EventHandler const Channel::stream_event_handler = {
    NULL /* audioMessage */,
    NULL /* videoMessage */,
    NULL /* rtmpCommandMessage */,
    NULL /* closed */,
    numWatchersChanged
};

void
Channel::numWatchersChanged (Count   const num_watchers,
                             void  * const _stream_data)
{
    logD_ (_func, num_watchers);

    StreamData * const stream_data = static_cast <StreamData*> (_stream_data);
    Channel * const self = stream_data->channel;

    self->mutex.lock ();
    if (stream_data != self->cur_stream_data /* ||
	stream_data->stream_closed */)
    {
	self->mutex.unlock ();
	return;
    }

    stream_data->num_watchers = num_watchers;

    if (num_watchers == 0) {
#warning TODO    if (!channel_opts->connect_on_demand || stream_stopped)
        if (!self->connect_on_demand_timer) {
            logD_ (_func, "starting timer, timeout: ", self->channel_opts->connect_on_demand_timeout);
            self->connect_on_demand_timer = self->timers->addTimer (
                    CbDesc<Timers::TimerCallback> (connectOnDemandTimerTick,
                                                   stream_data /* cb_data */,
                                                   self        /* coderef_container */,
                                                   stream_data /* ref_data */),
                    self->channel_opts->connect_on_demand_timeout,
                    false /* periodical */);
        }
    } else {
        if (self->connect_on_demand_timer) {
            self->timers->deleteTimer (self->connect_on_demand_timer);
            self->connect_on_demand_timer = NULL;
        }

        if (!self->media_source
            && !self->stream_stopped)
        {
            logD_ (_func, "connecting on demand");
            mt_unlocks (mutex) self->doRestartStream (true /* from_ondemand_reconnect */);
            return;
        }
    }

    self->mutex.unlock ();
}

void
Channel::connectOnDemandTimerTick (void * const _stream_data)
{
    logD_ (_func_);

    StreamData * const stream_data = static_cast <StreamData*> (_stream_data);
    Channel * const self = stream_data->channel;

    self->mutex.lock ();
    if (stream_data != self->cur_stream_data /* ||
	stream_data->stream_closed */)
    {
	self->mutex.unlock ();
	return;
    }

    if (stream_data->num_watchers == 0) {
        logD_ (_func, "disconnecting on timeout");
        self->closeStream (true /* replace_video_stream */);
    }

    self->mutex.unlock ();
}

mt_mutex (mutex) void
Channel::beginConnectOnDemand (bool const start_timer)
{
    assert (video_stream);

    if (!channel_opts->connect_on_demand || stream_stopped)
        return;

    video_stream->lock ();

    if (start_timer
        && video_stream->getNumWatchers_unlocked() == 0)
    {
        logD_ (_func, "starting timer, timeout: ", channel_opts->connect_on_demand_timeout);
        connect_on_demand_timer = timers->addTimer (
                CbDesc<Timers::TimerCallback> (connectOnDemandTimerTick,
                                               cur_stream_data /* cb_data */,
                                               this        /* coderef_container */,
                                               cur_stream_data /* ref_data */),
                channel_opts->connect_on_demand_timeout,
                false /* periodical */);
    }

    video_stream_events_sbn = video_stream->getEventInformer()->subscribe_unlocked (
            CbDesc<VideoStream::EventHandler> (
                    &stream_event_handler,
                    cur_stream_data /* cb_data */,
                    this        /* coderef_container */,
                    cur_stream_data /* ref_data */));

    video_stream->unlock ();
}

void
Channel::setStreamParameters (VideoStream * const mt_nonnull video_stream)
{
    Ref<StreamParameters> const stream_params = grab (new (std::nothrow) StreamParameters);
    if (channel_opts->no_audio)
        stream_params->setParam ("no_audio", "true");
    if (channel_opts->no_video)
        stream_params->setParam ("no_video", "true");

    video_stream->setStreamParameters (stream_params);
}

mt_mutex (mutex) void
Channel::createStream (Time const initial_seek)
{
    logD (ctl, _this_func_);

/* closeStream() is always called before createStream(), so this is unnecessary.
 *
    if (gst_stream) {
	gst_stream->releasePipeline ();
	gst_stream = NULL;
    }
 */

    stream_stopped = false;

    got_video = false;

    if (!cur_stream_data) {
        Ref<StreamData> const stream_data = grab (new (std::nothrow) StreamData (
                this, stream_ticket, stream_ticket_ref.ptr()));
        cur_stream_data = stream_data;
    }

    if (video_stream && video_stream_events_sbn) {
        video_stream->getEventInformer()->unsubscribe (video_stream_events_sbn);
        video_stream_events_sbn = NULL;
    }

    if (!video_stream) {
	video_stream = grab (new (std::nothrow) VideoStream);
        setStreamParameters (video_stream);

	logD_ (_func, "Calling moment->addVideoStream, stream_name: ", channel_opts->channel_name->mem());
	video_stream_key = moment->addVideoStream (video_stream, channel_opts->channel_name->mem());
    }

    Ref<VideoStream> bind_stream = video_stream;
    if (channel_opts->continuous_playback) {
        bind_stream = grab (new (std::nothrow) VideoStream);
        video_stream->bindToStream (bind_stream, bind_stream, true, true);
    }

    beginConnectOnDemand (true /* start_timer */);

    if (stream_start_time == 0)
	stream_start_time = getTime();

    media_source = moment->createMediaSource (
                           CbDesc<MediaSource::Frontend> (
                                   &media_source_frontend,
                                   cur_stream_data /* cb_data */,
                                   this            /* coderef_container */,
                                   cur_stream_data /* ref_data */),
                           timers,
                           page_pool,
                           bind_stream,
                           moment->getMixVideoStream(),
                           initial_seek,
                           channel_opts,
                           stream_spec->mem(),
                           is_chain,
                           force_transcode,
                           force_transcode_audio,
                           force_transcode_video);
    if (media_source) {
	media_source->ref ();
	GThread * const thread = g_thread_create (
#warning Not joinable?
		streamThreadFunc, media_source, FALSE /* joinable */, NULL /* error */);
	if (thread == NULL) {
	    logE_ (_func, "g_thread_create() failed");
	    media_source->unref ();
	}
    } else {
#warning Handle !media_source case
    }
}

gpointer
Channel::streamThreadFunc (gpointer const _media_source)
{
    MediaSource * const media_source = static_cast <MediaSource*> (_media_source);

    logD (ctl, _func_);

    updateTime ();
    media_source->createPipeline ();

    media_source->unref ();
    return (gpointer) 0;
}

mt_mutex (mutex) void
Channel::closeStream (bool const replace_video_stream)
{
    logD (ctl, _this_func_);

    got_video = false;

    if (connect_on_demand_timer) {
        timers->deleteTimer (connect_on_demand_timer);
        connect_on_demand_timer = NULL;
    }

    if (media_source) {
	{
	    MediaSource::TrafficStats traffic_stats;
	    media_source->getTrafficStats (&traffic_stats);

	    rx_bytes_accum += traffic_stats.rx_bytes;
	    rx_audio_bytes_accum += traffic_stats.rx_audio_bytes;
	    rx_video_bytes_accum += traffic_stats.rx_video_bytes;
	}

	media_source->releasePipeline ();
	media_source = NULL;
    }
    cur_stream_data = NULL;

    if (video_stream
        && replace_video_stream
	&& !channel_opts->keep_video_stream
        && !channel_opts->continuous_playback)
    {
	// TODO moment->replaceVideoStream() to swap video streams atomically
	moment->removeVideoStream (video_stream_key);
	video_stream->close ();

        if (video_stream_events_sbn) {
            video_stream->getEventInformer()->unsubscribe (video_stream_events_sbn);
            video_stream_events_sbn = NULL;
        }

	video_stream = NULL;

	if (replace_video_stream) {
            {
                assert (!cur_stream_data);

                Ref<StreamData> const stream_data = grab (new (std::nothrow) StreamData (
                        this, stream_ticket, stream_ticket_ref.ptr()));
                cur_stream_data = stream_data;
            }

	    video_stream = grab (new (std::nothrow) VideoStream);
            setStreamParameters (video_stream);

	    logD_ (_func, "Calling moment->addVideoStream, stream_name: ", channel_opts->channel_name->mem());
	    video_stream_key = moment->addVideoStream (video_stream, channel_opts->channel_name->mem());

// This seems to be unnecessary here.
//            // FIXME Connect on demand is not resumed if we don't get here. Bug.
//            beginConnectOnDemand (false /* start_timer */);
	}
    }

    logD_ (_func, "done");
}

mt_unlocks (mutex) void
Channel::doRestartStream (bool const from_ondemand_reconnect)
{
    logD (ctl, _this_func_);

    bool new_video_stream = false;
    if (media_source
        && !from_ondemand_reconnect)
    {
        closeStream (true /* replace_video_stream */);
        new_video_stream = true;
    }

    // TODO FIXME Set correct initial seek
    createStream (0 /* initial_seek */);

//    VirtRef const tmp_stream_ticket_ref = stream_ticket_ref;
//    void * const tmp_stream_ticket = stream_ticket;

    mutex.unlock ();

    if (new_video_stream)
        fireNewVideoStream ();
}

MediaSource::Frontend Channel::media_source_frontend = {
    streamError,
    streamEos,
    noVideo,
    gotVideo,
    streamStatusEvent
};

void
Channel::streamError (void * const _stream_data)
{
    StreamData * const stream_data = static_cast <StreamData*> (_stream_data);
    Channel * const self = stream_data->channel;

    logD (ctl, _self_func_);

    self->mutex.lock ();

    stream_data->stream_closed = true;
    if (stream_data != self->cur_stream_data) {
	self->mutex.unlock ();
	return;
    }

    VirtRef const tmp_stream_ticket_ref = self->stream_ticket_ref;
    void * const tmp_stream_ticket = self->stream_ticket;

    self->mutex.unlock ();

//    if (self->frontend)
//	self->frontend.call (self->frontend->error, tmp_stream_ticket);
}

void
Channel::streamEos (void * const _stream_data)
{
    StreamData * const stream_data = static_cast <StreamData*> (_stream_data);
    Channel * const self = stream_data->channel;

    logD (ctl, _self_func_);

    self->mutex.lock ();

    stream_data->stream_closed = true;
    if (stream_data != self->cur_stream_data) {
	self->mutex.unlock ();
	return;
    }

    VirtRef const tmp_stream_ticket_ref = self->stream_ticket_ref;
    void * const tmp_stream_ticket = self->stream_ticket;

    self->mutex.unlock ();

    Playback::AdvanceTicket * const advance_ticket =
            static_cast <Playback::AdvanceTicket*> (tmp_stream_ticket);
    self->playback.advance (advance_ticket);
}

void
Channel::noVideo (void * const _stream_data)
{
    StreamData * const stream_data = static_cast <StreamData*> (_stream_data);
    Channel * const self = stream_data->channel;

    self->mutex.lock ();
    if (stream_data != self->cur_stream_data ||
	stream_data->stream_closed)
    {
	self->mutex.unlock ();
	return;
    }

    mt_unlocks (mutex) self->doRestartStream ();
}

void
Channel::gotVideo (void * const _stream_data)
{
    StreamData * const stream_data = static_cast <StreamData*> (_stream_data);
    Channel * const self = stream_data->channel;

    logD (ctl, _self_func_);

    self->mutex.lock ();
    if (stream_data != self->cur_stream_data ||
	stream_data->stream_closed)
    {
	self->mutex.unlock ();
	return;
    }

    self->got_video = true;
    self->mutex.unlock ();
}

void
Channel::streamStatusEvent (void * const _stream_data)
{
    StreamData * const stream_data = static_cast <StreamData*> (_stream_data);
    Channel * const self = stream_data->channel;

    logD (ctl, _self_func_);

    self->deferred_reg.scheduleTask (&self->deferred_task, false /* permanent */);
}

bool
Channel::deferredTask (void * const _self)
{
    Channel * const self = static_cast <Channel*> (_self);

    logD (ctl, _self_func_);

  {
    self->mutex.lock ();
    if (!self->media_source) {
	self->mutex.unlock ();
	goto _return;
    }

    Ref<MediaSource> const tmp_media_source = self->media_source;
    self->mutex.unlock ();

    tmp_media_source->reportStatusEvents ();
  }

_return:
    return false /* Do not reschedule */;
}

// If @is_chain is 'true', then @stream_spec is a chain spec with gst-launch
// syntax. Otherwise, @stream_spec is an uri for uridecodebin2.
void
Channel::beginVideoStream (ConstMemory      const stream_spec,
                           bool             const is_chain,
                           bool             const force_transcode,
                           bool             const force_transcode_audio,
                           bool             const force_transcode_video,
                           void           * const stream_ticket,
                           VirtReferenced * const stream_ticket_ref,
                           Time             const seek)
{
    logD (ctl, _this_func, "is_chain: ", is_chain);

    mutex.lock ();

    if (media_source)
	closeStream (true /* replace_video_stream */);

    this->stream_spec = grab (new (std::nothrow) String (stream_spec));
    this->is_chain = is_chain;
    this->force_transcode = force_transcode;
    this->force_transcode_audio = force_transcode_audio;
    this->force_transcode_video = force_transcode_video;

    this->stream_ticket = stream_ticket;
    this->stream_ticket_ref = stream_ticket_ref;

    createStream (seek);

    mutex.unlock ();
}

void
Channel::endVideoStream ()
{
    logD (ctl, _this_func_);

    mutex.lock ();

    stream_stopped = true;

    if (media_source)
	closeStream (true /* replace_video_stream */);

    mutex.unlock ();
}

void
Channel::restartStream ()
{
    logD (ctl, _this_func_);

    mutex.lock ();
    mt_unlocks (mutex) doRestartStream ();
}

bool
Channel::isSourceOnline ()
{
    mutex.lock ();
    bool const res = got_video;
    mutex.unlock ();
    return res;
}

void
Channel::getTrafficStats (TrafficStats * const ret_traffic_stats)
{
  StateMutexLock l (&mutex);

    MediaSource::TrafficStats stream_tstat;
    if (media_source)
	media_source->getTrafficStats (&stream_tstat);
    else
	stream_tstat.reset ();

    ret_traffic_stats->rx_bytes = rx_bytes_accum + stream_tstat.rx_bytes;
    ret_traffic_stats->rx_audio_bytes = rx_audio_bytes_accum + stream_tstat.rx_audio_bytes;
    ret_traffic_stats->rx_video_bytes = rx_video_bytes_accum + stream_tstat.rx_video_bytes;
    {
	Time const cur_time = getTime();
	if (cur_time > stream_start_time)
	    ret_traffic_stats->time_elapsed = cur_time - stream_start_time;
	else
	    ret_traffic_stats->time_elapsed = 0;
    }
}

void
Channel::resetTrafficStats ()
{
  StateMutexLock l (&mutex);

    if (media_source)
	media_source->resetTrafficStats ();

    rx_bytes_accum = 0;
    rx_audio_bytes_accum = 0;
    rx_video_bytes_accum = 0;

    stream_start_time = getTime();
}

mt_const void
Channel::init (MomentServer   * const mt_nonnull moment,
               ChannelOptions * const mt_nonnull channel_opts)
{
    this->channel_opts = channel_opts;

    this->moment = moment;
    this->timers = moment->getServerApp()->getServerContext()->getMainThreadContext()->getTimers();
    this->page_pool = moment->getPagePool();

    deferred_reg.setDeferredProcessor (moment->getServerApp()->getServerContext()->getMainThreadContext()->getDeferredProcessor());

    playback.init (CbDesc<Playback::Frontend> (&playback_frontend, this, this),
                   moment->getServerApp()->getServerContext()->getMainThreadContext()->getTimers(),
                   channel_opts->min_playlist_duration_sec);
}

// TODO Unused?
#if 0
void
GstStreamCtl::release ()
{
    mutex.lock ();
    closeStream (false /* replace_video_stream */);
    mutex.unlock ();
}
#endif

Channel::Channel ()
    : event_informer (this /* coderef_container */, &mutex),
      playback       (this /* coderef_container */),
        
      moment    (this /* coderef_container */),
      timers    (this /* coderef_container */),
      page_pool (this /* coderef_container */),

      stream_ticket (NULL),

      stream_stopped (false),
      got_video (false),

      connect_on_demand_timer (NULL),

      stream_start_time (0),

      rx_bytes_accum (0),
      rx_audio_bytes_accum (0),
      rx_video_bytes_accum (0)
{
    logD (ctl, _this_func_);

    deferred_task.cb = CbDesc<DeferredProcessor::TaskCallback> (
	    deferredTask, this /* cb_data */, this /* coderef_container */);
}

Channel::~Channel ()
{
    logD (ctl, _this_func_);

    mutex.lock ();

    closeStream (false /* replace_video_stream */);

#if 0
    if (gst_stream) {
        gst_stream->releasePipeline ();
        gst_stream = NULL;
    }
#endif

    mutex.unlock ();

    deferred_reg.release ();
}

}

