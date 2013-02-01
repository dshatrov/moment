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


#ifndef MOMENT__CHANNEL_MANAGER__H__
#define MOMENT__CHANNEL_MANAGER__H__


#include <libmary/libmary.h>
#include <moment/channel.h>
#include <moment/moment_server.h>


namespace Moment {

using namespace M;

class ChannelManager : public Object
{
private:
    StateMutex mutex;

    class ConfigItem : public StReferenced
    {
    public:
        Ref<MConfig::Config> config;
        Ref<Channel> channel;
        StRef<String> channel_name;
        StRef<String> channel_title;
    };

    mt_const Ref<MomentServer> moment;
    mt_const DataDepRef<PagePool> page_pool;

    typedef StringHash< StRef<ConfigItem> > ItemHash;
    mt_mutex (mutex) ItemHash item_hash;

    mt_mutex (mutex) Ref<ChannelOptions> default_channel_opts;

  mt_iface (MomentServer::HttpRequestHandler)
      static MomentServer::HttpRequestHandler admin_http_handler;

      static MomentServer::HttpRequestResult adminHttpRequest (HttpRequest * mt_nonnull req,
                                                               Sender      * mt_nonnull conn_sender,
                                                               Memory       msg_body,
                                                               void        *cb_data);
  mt_iface_end

  mt_iface (MomentServer::HttpRequestHandler)
      static MomentServer::HttpRequestHandler server_http_handler;

      static MomentServer::HttpRequestResult serverHttpRequest (HttpRequest * mt_nonnull req,
                                                                Sender      * mt_nonnull conn_sender,
                                                                Memory       msg_body,
                                                                void        *cb_data);
  mt_iface_end

public:
    Result loadConfigFull ();

    Result loadConfigItem (ConstMemory item_name,
                           ConstMemory item_path);

    void setDefaultChannelOptions (ChannelOptions * mt_nonnull channel_opts);

    mt_const void init (MomentServer * mt_nonnull moment,
                        PagePool     * mt_nonnull page_pool);

    ChannelManager ();
};

}


#endif /* MOMENT__CHANNEL_MANAGER__H__ */

