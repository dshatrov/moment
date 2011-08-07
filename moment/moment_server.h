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


#ifndef __MOMENT__SERVER__H__
#define __MOMENT__SERVER__H__


#include <libmary/libmary.h>
#include <mconfig/mconfig.h>


#include <moment/rtmp_connection.h>
#include <moment/video_stream.h>


namespace Moment {

using namespace M;

// Only one MomentServer object may be initialized during program's lifetime.
// This limitation comes form loadable modules support.
class MomentServer
{
public:
    // TODO Unused
    struct VideoStreamHandler
    {
	Result (*videoStreamOpened) (VideoStream * mt_nonnull video_stream);
    };

private:
    ServerApp *server_app;
    PagePool *page_pool;
    HttpService *http_service;
    MConfig::Config *config;

    static MomentServer *instance;

    class VideoStreamEntry
    {
    public:
	Ref<VideoStream> video_stream;

	VideoStreamEntry (VideoStream * const video_stream)
	    : video_stream (video_stream)
	{
	}
    };

    typedef StringHash<VideoStreamEntry> VideoStreamHash;
    VideoStreamHash video_stream_hash;

    StateMutex mutex;

    mt_throws Result loadModules ();

public:
  // Getting pointers to common objects

    ServerApp* getServerApp ();

    PagePool* getPagePool ();

    HttpService* getHttpService ();

    MConfig::Config* getConfig ();

    static MomentServer* getInstance ();

  // Video stream handlers

    void addVideoStreamHandler (Cb<VideoStreamHandler> const &cb,
				ConstMemory const &path_prefix);

  // Get/add/remove video streams

    class VideoStreamKey
    {
	friend class MomentServer;
    private:
	VideoStreamHash::EntryKey entry_key;
	VideoStreamKey (VideoStreamHash::EntryKey entry_key) : entry_key (entry_key) {}
    public:
	operator bool () const { return entry_key; }
	VideoStreamKey () {}
    };

    Ref<VideoStream> getVideoStream (ConstMemory const &path);

    VideoStreamKey addVideoStream (VideoStream * const video_stream,
				   ConstMemory const &path);

    void removeVideoStream (VideoStreamKey video_stream_key);

  // Initialization

    Result init (ServerApp       * mt_nonnull server_app,
		 PagePool        * mt_nonnull page_pool,
		 HttpService     * mt_nonnull http_service,
		 MConfig::Config * mt_nonnull config);

    MomentServer ();

    ~MomentServer ();    
};

}


#endif /* __MOMENT__SERVER__H__ */

