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


#include <moment/moment_server.h>


namespace Moment {

using namespace M;

MomentServer* MomentServer::instance = NULL;

mt_throws Result
MomentServer::loadModules ()
{
    ConstMemory module_path = config->getString ("moment/module_path");
    if (module_path.len() == 0)
	module_path = LIBMOMENT_PREFIX "/moment-1.0";

    logD_ (_func, "MODULE PATH: ", module_path);

    Ref<Vfs> const vfs = Vfs::createDefaultLocalVfs (module_path);
    if (!vfs)
	return Result::Failure;

    Ref<Vfs::Directory> const dir = vfs->openDirectory (ConstMemory());
    if (!dir)
	return Result::Failure;

    StringHash<EmptyBase> loaded_names;
    for (;;) {
	Ref<String> dir_entry;
	if (!dir->getNextEntry (dir_entry))
	    return Result::Failure;
	if (!dir_entry)
	    break;

	Ref<String> const stat_path = makeString (module_path, "/", dir_entry->mem());

	ConstMemory const entry_name = stat_path->mem();

	Ref<Vfs::FileStat> const stat_data = vfs->stat (dir_entry->mem());
	if (!stat_data) {
	    logE_ (_func, "Could not stat ", stat_path);
	    continue;
	}

	// TODO Find rightmost slash, then skip one dot.
	ConstMemory module_name = entry_name;
	{
	    void *dot_ptr = memchr ((void*) entry_name.mem(), '.', entry_name.len());
	    // XXX Dirty.
	    // Skipping the first dot (belongs to "moment-1.0" substring).
	    if (dot_ptr)
		dot_ptr = memchr ((void*) ((Byte const *) dot_ptr + 1), '.', entry_name.len() - ((Byte const *) dot_ptr - entry_name.mem()) - 1);
	    // Skipping the second dot (-1.0 in library version).
	    if (dot_ptr)
		dot_ptr = memchr ((void*) ((Byte const *) dot_ptr + 1), '.', entry_name.len() - ((Byte const *) dot_ptr - entry_name.mem()) - 1);

	    if (dot_ptr)
		module_name = entry_name.region (0, (Byte const *) dot_ptr - entry_name.mem());
	}

	if (stat_data->file_type == Vfs::FileType::RegularFile &&
	    !loaded_names.lookup (module_name))
	{
	    loaded_names.add (module_name, EmptyBase());

	    logD_ (_func, "LOADING MODULE ", module_name);

	    if (!loadModule (module_name))
		logE_ (_func, "Could not load module ", module_name, ": ", exc->toString());
	}
    }

    return Result::Success;
}

ServerApp*
MomentServer::getServerApp ()
{
    return server_app;
}

PagePool*
MomentServer::getPagePool ()
{
    return page_pool;
}

HttpService*
MomentServer::getHttpService ()
{
    return http_service;
}

MConfig::Config* 
MomentServer::getConfig ()
{
    return config;
}

MomentServer*
MomentServer::getInstance ()
{
    return instance;
}

void
MomentServer::addVideoStreamHandler (Cb<VideoStreamHandler> const &cb,
				     ConstMemory const &path_prefix)
{
  // TODO
}

Ref<VideoStream>
MomentServer::getVideoStream (ConstMemory const &path)
{
  StateMutexLock l (&mutex);

    VideoStreamHash::EntryKey const entry = video_stream_hash.lookup (path);
    if (!entry)
	return NULL;

    return entry.getData().video_stream;
}

MomentServer::VideoStreamKey
MomentServer::addVideoStream (VideoStream * const  video_stream,
			      ConstMemory   const &path)
{
  StateMutexLock l (&mutex);
    return video_stream_hash.add (path, video_stream);
}

void
MomentServer::removeVideoStream (VideoStreamKey const video_stream_key)
{
    mutex.lock ();
    video_stream_hash.remove (video_stream_key.entry_key);
    mutex.unlock ();
}

Result
MomentServer::init (ServerApp       * const server_app,
		    PagePool        * const page_pool,
		    HttpService     * const http_service,
		    MConfig::Config * const mt_nonnull config)
{
    this->server_app = server_app;
    this->page_pool = page_pool;
    this->http_service = http_service;
    this->config = config;

    if (!loadModules ())
	logE_ (_func, "Could not load modules");

    return Result::Success;
}

MomentServer::MomentServer ()
    : server_app (NULL),
      page_pool (NULL),
      http_service (NULL),
      config (NULL)
{
    instance = this;
}

MomentServer::~MomentServer ()
{
    mutex.lock ();
    mutex.unlock ();
}

}

