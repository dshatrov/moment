/*  Moment Video Server - High performance media server
    Copyright (C) 2013 Dmitry Shatrov
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


#include <mconfig/mconfig.h>
#include <moment/libmoment.h>

#include <moment/channel_manager.h>


using namespace M;

namespace Moment {

MomentServer::HttpRequestHandler ChannelManager::admin_http_handler = {
    adminHttpRequest
};

MomentServer::HttpRequestResult
ChannelManager::adminHttpRequest (HttpRequest * const mt_nonnull req,
                                  Sender      * const mt_nonnull conn_sender,
                                  Memory        const /* msg_body */,
                                  void        * const _self)
{
    ChannelManager * const self = static_cast <ChannelManager*> (_self);

    MOMENT_SERVER__HEADERS_DATE

    if (req->getNumPathElems() >= 2
        && equal (req->getPath (1), "reload_channel"))
    {
        // TODO get channel name attribute

        logD_ (_func, "reload_channel");

        ConstMemory const item_name = req->getParameter ("conf_file");
        if (item_name.len() == 0) {
            ConstMemory const reply_body = "400 Bad Request: no conf_file parameter";
            conn_sender->send (self->page_pool,
                               true /* do_flush */,
                               MOMENT_SERVER__400_HEADERS (reply_body.len()),
                               "\r\n",
                               reply_body);
            logA_ ("moment__channel_manager 400 ", req->getClientAddress(), " ", req->getRequestLine());
            goto _return;
        }

        ConstMemory const dir_name = "/opt/moment/conf.d";
        StRef<String> const path = st_makeString (dir_name, "/", item_name);

        if (!self->loadConfigItem (item_name, path->mem())) {
            ConstMemory const reply_body = "500 Internal Server Error: loadConfigItem() failed";
            conn_sender->send (self->page_pool,
                               true /* do_flush */,
                               MOMENT_SERVER__500_HEADERS (reply_body.len()),
                               "\r\n",
                               reply_body);
            logA_ ("moment__channel_manager 500 ", req->getClientAddress(), " ", req->getRequestLine());
            goto _return;
        }

        ConstMemory const reply_body = "OK";
        conn_sender->send (self->page_pool,
                           true /* do_flush */,
                           MOMENT_SERVER__OK_HEADERS ("text/plain", reply_body.len()),
                           "\r\n",
                           reply_body);
        logA_ ("moment__channel_manager OK ", req->getClientAddress(), " ", req->getRequestLine());
    } else {
        return MomentServer::HttpRequestResult::NotFound;
    }

_return:
    if (!req->getKeepalive())
	conn_sender->closeAfterFlush ();

    return MomentServer::HttpRequestResult::Success;
}

MomentServer::HttpRequestHandler ChannelManager::server_http_handler = {
    serverHttpRequest
};

MomentServer::HttpRequestResult
ChannelManager::serverHttpRequest (HttpRequest * const mt_nonnull req,
                                   Sender      * const mt_nonnull conn_sender,
                                   Memory        const /* msg_body */,
                                   void        * const _self)
{
    ChannelManager * const self = static_cast <ChannelManager*> (_self);

    MOMENT_SERVER__HEADERS_DATE

    if (req->getNumPathElems() >= 2
        && equal (req->getPath (1), "playlist.json"))
    {
        logD_ (_func, "playlist.json");

        PagePool::PageListHead page_list;

        static char const prefix [] = "[\n";
        static char const suffix [] = "]\n";

        self->page_pool->getFillPages (&page_list, prefix);

        self->mutex.lock ();
	{
            bool use_rtmpt_proto = false;
// TODO serve_playlist_json, playlist_json_protocol config parameters
//
//            if (equal (self->playlist_json_protocol->mem(), "rtmpt"))
//                use_rtmpt_proto = true;

            // TODO Update once on config reload.
            StRef<String> this_rtmp_server_addr;
            StRef<String> this_rtmpt_server_addr;
            {
                self->moment->configLock ();
                MomentServer::VarHash * const var_hash = self->moment->getDefaultVarHash_unlocked ();

                if (MomentServer::VarHashEntry * const entry = var_hash->lookup (ConstMemory ("RtmpAddr")))
                    this_rtmp_server_addr = st_grab (new (std::nothrow) String (entry->var->getValue()));
                else
                    this_rtmp_server_addr = st_grab (new (std::nothrow) String ("127.0.0.1:1935"));

                if (MomentServer::VarHashEntry * const entry = var_hash->lookup (ConstMemory ("RtmptAddr")))
                    this_rtmpt_server_addr = st_grab (new (std::nothrow) String (entry->var->getValue()));
                else
                    this_rtmpt_server_addr = st_grab (new (std::nothrow) String ("127.0.0.1:8080"));

                self->moment->configUnlock ();
            }

	    ItemHash::iterator iter (self->item_hash);
	    while (!iter.done()) {
		ConfigItem * const item = iter.next ()->ptr();

		StRef<String> const channel_line = st_makeString (
                        "[ \"", item->channel_title, "\", "
                        "\"", (use_rtmpt_proto ? ConstMemory ("rtmpt://") : ConstMemory ("rtmp://")),
                                (use_rtmpt_proto ? this_rtmpt_server_addr->mem() : this_rtmp_server_addr->mem()),
                                "/live/", item->channel_name->mem(), "\", "
                        "\"", item->channel_name->mem(), "\" ],\n");

		self->page_pool->getFillPages (&page_list, channel_line->mem());

                logD_ (_func, "playlist.json line: ", channel_line->mem());
	    }
	}
        self->mutex.unlock ();

        self->page_pool->getFillPages (&page_list, suffix);

	Size content_len = 0;
	{
	    PagePool::Page *page = page_list.first;
	    while (page) {
		content_len += page->data_len;
		page = page->getNextMsgPage();
	    }
	}

	conn_sender->send (self->page_pool,
			   false /* do_flush */,
			   MOMENT_SERVER__OK_HEADERS ("text/html", content_len),
			   "\r\n");
	conn_sender->sendPages (self->page_pool, &page_list, true /* do_flush */);

	logA_ ("moment__channel_manager 200 ", req->getClientAddress(), " ", req->getRequestLine());
    } else {
        return MomentServer::HttpRequestResult::NotFound;
    }

    if (!req->getKeepalive())
	conn_sender->closeAfterFlush ();

    return MomentServer::HttpRequestResult::Success;
}

Result
ChannelManager::loadConfigFull ()
{
    logD_ (_func_);

    ConstMemory const dir_name = "/opt/moment/conf.d";
    Ref<Vfs> const vfs = Vfs::createDefaultLocalVfs (dir_name);

    Ref<Vfs::Directory> const dir = vfs->openDirectory ("");
    if (!dir) {
        logE_ (_func, "could not open ", dir_name, " directory: ", exc->toString());
        return Result::Failure;
    }

    for (;;) {
        Ref<String> filename;
        if (!dir->getNextEntry (filename)) {
            logE_ (_func, "Vfs::Directory::getNextEntry() failed: ", exc->toString());
            return Result::Failure;
        }
        if (!filename)
            break;

        if (equal (filename->mem(), ".") || equal (filename->mem(), ".."))
            continue;

        StRef<String> const path = st_makeString (dir_name, "/", filename->mem());
        if (!loadConfigItem (filename->mem(), path->mem()))
            return Result::Failure;
    }

    return Result::Success;
}

static ConstMemory itemToStreamName (ConstMemory const item_name)
{
    ConstMemory stream_name = item_name;
    for (Size i = stream_name.len(); i > 0; --i) {
        if (stream_name.mem() [i - 1] == '.') {
            stream_name = stream_name.region (0, i - 1);
            break;
        }
    }

    return stream_name;
}

static Result
parseChannelConfig (MConfig::Section * const mt_nonnull section,
                    ConstMemory        const config_item_name,
                    ChannelOptions   * const default_opts,
                    ChannelOptions   * const mt_nonnull opts,
                    PlaybackItem     * const mt_nonnull item)
{
    char const opt_name__name []                     = "name";
    char const opt_name__title[]                     = "title";
    char const opt_name__desc []                     = "desc";
    char const opt_name__chain[]                     = "chain";
    char const opt_name__uri  []                     = "uri";
    char const opt_name__playlist[]                  = "playlist";
    char const opt_name__keep_video_stream[]         = "keep_video_stream";
    char const opt_name__continuous_playback[]       = "continous_playback";
    char const opt_name__record_path[]               = "record_path";
    char const opt_name__connect_on_demand[]         = "connect_on_demand";
    char const opt_name__connect_on_demand_timeout[] = "connect_on_demand_timeout";
    char const opt_name__no_video_timeout[]          = "no_video_timeout";
    char const opt_name__min_playlist_duration[]     = "min_playlist_duration";
    char const opt_name__no_audio[]                  = "no_audio";
    char const opt_name__no_video[]                  = "no_video";
    char const opt_name__force_transcode[]           = "force_transcode";
    char const opt_name__force_transcode_audio[]     = "force_transcode_audio";
    char const opt_name__force_transcode_video[]     = "force_transcode_video";
    char const opt_name__send_metadata[]             = "send_metadata";
    char const opt_name__enable_prechunking[]        = "enable_prechunking";
// TODO resize and transcoding settings
#if 0
    char const opt_name__width[]                     = "width";
    char const opt_name__height[]                    = "height";
    char const opt_name__bitrate[]                   = "bitrate";
#endif

    ConstMemory channel_name = itemToStreamName (config_item_name);
    {
        MConfig::Section::attribute_iterator attr_iter (*section);
        MConfig::Attribute *name_attr = NULL;
        if (!attr_iter.done()) {
            MConfig::Attribute * const attr = attr_iter.next ();
            if (!attr->hasValue()) {
                channel_name = attr->getName();
                name_attr = attr;
            }
        }

        if (MConfig::Attribute * const attr = section->getAttribute (opt_name__name)) {
            if (attr != name_attr)
                channel_name = attr->getValue ();
        }
    }
    logD_ (_func, opt_name__name, ": ", channel_name);

    ConstMemory channel_title = channel_name;
    if (MConfig::Option * const opt = section->getOption (opt_name__title)) {
        if (opt->getValue())
            channel_title = opt->getValue()->mem();
    }
    logD_ (_func, opt_name__title, ": ", channel_title);

    ConstMemory channel_desc = default_opts->channel_desc->mem();
    if (MConfig::Option * const opt = section->getOption (opt_name__desc)) {
        if (opt->getValue())
            channel_desc = opt->getValue()->mem();
    }
    logD_ (_func, opt_name__desc, ": ", channel_desc);

    PlaybackItem::SpecKind spec_kind = PlaybackItem::SpecKind::None;
    ConstMemory stream_spec;
    {
        int num_set_opts = 0;

        if (MConfig::Option * const opt = section->getOption (opt_name__chain)) {
            if (opt->getValue()) {
                stream_spec = opt->getValue()->mem();
                spec_kind = PlaybackItem::SpecKind::Chain;
                ++num_set_opts;
            }
            logD_ (_func, opt_name__chain, ": ", stream_spec);
        }

        if (MConfig::Option * const opt = section->getOption (opt_name__uri)) {
            if (opt->getValue()) {
                stream_spec = opt->getValue()->mem();
                spec_kind = PlaybackItem::SpecKind::Uri;
                ++num_set_opts;
            }
            logD_ (_func, opt_name__uri, ": ", stream_spec);
        }

        if (MConfig::Option * const opt = section->getOption (opt_name__playlist)) {
            if (opt->getValue()) {
                stream_spec = opt->getValue()->mem();
                spec_kind = PlaybackItem::SpecKind::Playlist;
                ++num_set_opts;
            }
            logD_ (_func, opt_name__playlist, ": ", stream_spec);
        }

        if (num_set_opts > 1)
            logW_ (_func, "only one of uri/chain/playlist options should be specified");
    }

    bool keep_video_stream = default_opts->keep_video_stream;
    if (!configSectionGetBoolean (section,
                                  opt_name__keep_video_stream,
                                  &keep_video_stream,
                                  keep_video_stream))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__keep_video_stream, ": ", keep_video_stream);

    bool continuous_playback = default_opts->continuous_playback;
    if (!configSectionGetBoolean (section,
                                  opt_name__continuous_playback,
                                  &continuous_playback,
                                  continuous_playback))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__continuous_playback, ": ", continuous_playback);

    ConstMemory record_path = default_opts->record_path->mem();
    bool got_record_path = false;
    if (MConfig::Option * const opt = section->getOption (opt_name__record_path)) {
        if (opt->getValue()) {
            record_path = opt->getValue()->mem();
            got_record_path = true;
        }
    }
    logD_ (_func, opt_name__record_path, ": ", record_path);

    bool connect_on_demand = default_opts->connect_on_demand;
    if (!configSectionGetBoolean (section,
                                  opt_name__connect_on_demand,
                                  &connect_on_demand,
                                  connect_on_demand))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__connect_on_demand, ": ", connect_on_demand);

    Uint64 connect_on_demand_timeout = default_opts->connect_on_demand_timeout;
    if (!configSectionGetUint64 (section,
                                 opt_name__connect_on_demand_timeout,
                                 &connect_on_demand_timeout,
                                 connect_on_demand_timeout))
    {
        return Result::Failure;
    }

    Uint64 no_video_timeout = default_opts->no_video_timeout;
    if (!configSectionGetUint64 (section,
                                 opt_name__no_video_timeout,
                                 &no_video_timeout,
                                 no_video_timeout))
    {
        return Result::Failure;
    }

    Uint64 min_playlist_duration = default_opts->min_playlist_duration_sec;
    if (!configSectionGetUint64 (section,
                                 opt_name__min_playlist_duration,
                                 &min_playlist_duration,
                                 min_playlist_duration))
    {
        return Result::Failure;
    }

// TODO PushAgent    ConstMmeory push_uri;

    bool no_audio = default_opts->default_item->no_audio;
    if (!configSectionGetBoolean (section, opt_name__no_audio, &no_audio, no_audio))
        return Result::Failure;
    logD_ (_func, opt_name__no_audio, ": ", no_audio);

    bool no_video = default_opts->default_item->no_video;
    if (!configSectionGetBoolean (section, opt_name__no_video, &no_video, no_video))
        return Result::Failure;
    logD_ (_func, opt_name__no_video, ": ", no_video);

    bool force_transcode = default_opts->default_item->force_transcode;
    if (!configSectionGetBoolean (section,
                                  opt_name__force_transcode,
                                  &force_transcode,
                                  force_transcode))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__force_transcode, ": ", force_transcode);

    bool force_transcode_audio = default_opts->default_item->force_transcode_audio;
    if (!configSectionGetBoolean (section,
                                  opt_name__force_transcode_audio,
                                  &force_transcode_audio,
                                  force_transcode_audio))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__force_transcode_audio, ": ", force_transcode_audio);

    bool force_transcode_video = default_opts->default_item->force_transcode_video;
    if (!configSectionGetBoolean (section,
                                  opt_name__force_transcode_video,
                                  &force_transcode_video,
                                  force_transcode_video))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__force_transcode_video, ": ", force_transcode_video);

    bool send_metadata = default_opts->default_item->send_metadata;
    if (!configSectionGetBoolean (section,
                                  opt_name__send_metadata,
                                  &send_metadata,
                                  send_metadata))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__send_metadata, ": ", send_metadata);

    bool enable_prechunking = default_opts->default_item->enable_prechunking;
    if (!configSectionGetBoolean (section,
                                  opt_name__enable_prechunking,
                                  &enable_prechunking,
                                  enable_prechunking))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__enable_prechunking, ": ", enable_prechunking);

    opts->channel_name  = st_grab (new (std::nothrow) String (channel_name));
    opts->channel_title = st_grab (new (std::nothrow) String (channel_title));
    opts->channel_desc  = st_grab (new (std::nothrow) String (channel_desc));

    opts->keep_video_stream  = keep_video_stream;
    opts->continuous_playback = continuous_playback;

    opts->recording = got_record_path;
    opts->record_path = st_grab (new (std::nothrow) String (record_path));

    opts->connect_on_demand = connect_on_demand;
    opts->connect_on_demand_timeout = connect_on_demand_timeout;

    opts->no_video_timeout = no_video_timeout;
    opts->min_playlist_duration_sec = min_playlist_duration;

    item->stream_spec = st_grab (new (std::nothrow) String (stream_spec));
    item->spec_kind = spec_kind;

    item->no_audio = no_audio;
    item->no_video = no_video;

    item->force_transcode = force_transcode;
    item->force_transcode_audio = force_transcode_audio;
    item->force_transcode_video = force_transcode_video;

    item->send_metadata = /* TODO send_metadata */ false;
    item->enable_prechunking = enable_prechunking;

    return Result::Success;
}

Result
ChannelManager::loadConfigItem (ConstMemory const item_name,
                                ConstMemory const item_path)
{
    logD_ (_func, "item_name: ", item_name, ", item_path: ", item_path);

    Ref<MConfig::Config> const config = grab (new (std::nothrow) MConfig::Config);
    if (!MConfig::parseConfig (item_path, config)) {
        logE_ (_func, "could not parse config file ", item_path);

        mutex.lock ();
        if (ItemHash::EntryKey const old_item_key = item_hash.lookup (item_name)) {
            StRef<ConfigItem> const item = old_item_key.getData();
            item->channel->endVideoStream ();
            item_hash.remove (old_item_key);
        }
        mutex.unlock ();

        return Result::Failure;
    }

    Ref<VideoStream> const stream = grab (new (std::nothrow) VideoStream);

    mutex.lock ();

    StRef<ConfigItem> item;
    if (ItemHash::EntryKey const old_item_key = item_hash.lookup (item_name)) {
        item = old_item_key.getData();
        item->channel->endVideoStream ();
    } else {
        item = st_grab (new (std::nothrow) ConfigItem);
        item_hash.add (item_name, item);
    }

    Ref<ChannelOptions> const channel_opts = grab (new (std::nothrow) ChannelOptions);
    *channel_opts = *default_channel_opts;

    Ref<PlaybackItem> const playback_item = grab (new (std::nothrow) PlaybackItem);
    channel_opts->default_item = playback_item;
    *playback_item = *default_channel_opts->default_item;

    parseChannelConfig (config->getRootSection(),
                        item_name,
                        default_channel_opts,
                        channel_opts,
                        playback_item);
    item->channel_name  = st_grab (new (std::nothrow) String (channel_opts->channel_name->mem()));
    item->channel_title = st_grab (new (std::nothrow) String (channel_opts->channel_title->mem()));

    logD_ (_func, "ChannelOptions: ");
    channel_opts->dump();

    logD_ (_func, "PlaybackItem: ");
    playback_item->dump();

#warning Don't substitute existing channel
    item->channel = grab (new (std::nothrow) Channel);
    item->channel->init (moment, channel_opts);

    item->config = config;

    // Calling with 'mutex' locked for extra safety.
    item->channel->beginVideoStream (
            playback_item,
            NULL /* stream_ticket */,
            NULL /* stream_ticket_ref */);

    mutex.unlock ();

    return Result::Success;
}

void
ChannelManager::setDefaultChannelOptions (ChannelOptions * const mt_nonnull channel_opts)
{
    mutex.lock ();
    default_channel_opts = channel_opts;
    mutex.unlock ();
}

mt_const void
ChannelManager::init (MomentServer * const mt_nonnull moment,
                      PagePool     * const mt_nonnull page_pool)
{
    this->moment = moment;
    this->page_pool = page_pool;

    moment->addAdminRequestHandler (
            CbDesc<MomentServer::HttpRequestHandler> (&admin_http_handler, this, this));
    moment->addServerRequestHandler (
            CbDesc<MomentServer::HttpRequestHandler> (&server_http_handler, this, this));
}

ChannelManager::ChannelManager ()
    : page_pool (this)
{
}

}
