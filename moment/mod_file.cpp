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


#include <libmary/types.h>
#include <cstring>

#include <mycpp/list.h>
#include <libmary/module_init.h>
#include <moment/libmoment.h>


// TODO These header macros are the same as in rtmpt_server.cpp
#define MOMENT_FILE__HEADERS_DATE \
	Byte date_buf [timeToString_BufSize]; \
	Size const date_len = timeToString (Memory::forObject (date_buf), getUnixtime());

#define MOMENT_FILE__COMMON_HEADERS \
	"Server: Moment/1.0\r\n" \
	"Date: ", ConstMemory (date_buf, date_len), "\r\n" \
	"Connection: Keep-Alive\r\n" \
	"Cache-Control: no-cache\r\n"

#define MOMENT_FILE__OK_HEADERS(mime_type, content_length) \
	"HTTP/1.1 200 OK\r\n" \
	MOMENT_FILE__COMMON_HEADERS \
	"Content-Type: ", (mime_type), "\r\n" \
	"Content-Length: ", (content_length), "\r\n"

#define MOMENT_FILE__404_HEADERS(content_length) \
	"HTTP/1.1 404 Not found\r\n" \
	MOMENT_FILE__COMMON_HEADERS \
	"Content-Type: text/plain\r\n" \
	"Content-Length: ", (content_length), "\r\n"

#define MOMENT_FILE__400_HEADERS(content_length) \
	"HTTP/1.1 400 Bad Request\r\n" \
	MOMENT_FILE__COMMON_HEADERS \
	"Content-Type: text/plain\r\n" \
	"Content-Length: ", (content_length), "\r\n"

#define MOMENT_FILE__500_HEADERS(content_length) \
	"HTTP/1.1 500 Internal Server Error\r\n" \
	MOMENT_FILE__COMMON_HEADERS \
	"Content-Type: text/plain\r\n" \
	"Content-Length: ", (content_length), "\r\n"


using namespace M;

namespace Moment {

namespace {

class PathEntry : public BasicReferenced
{
public:
    Ref<String> path;
    Ref<String> prefix;
};

MyCpp::List< Ref<PathEntry> > path_list;

PagePool *page_pool = NULL;

Result httpRequest (HttpRequest  * const mt_nonnull req,
		    Sender       * const mt_nonnull conn_sender,
		    Memory const & /* msg_body */,
		    void        ** const mt_nonnull /* ret_msg_data */,
		    void         * const _path_entry)
{
    PathEntry * const path_entry = static_cast <PathEntry*> (_path_entry);

//    logD_ (_func, "HTTP request: ", req->getRequestLine());

  // TODO On Linux, we could do a better job with sendfile() or splice().

    ConstMemory file_path;
    {
	ConstMemory full_path = req->getFullPath();
	if (full_path.len() > 0
	    && full_path.mem() [0] == '/')
	{
	    full_path = full_path.region (1);
	}

	ConstMemory const prefix = path_entry->prefix->mem();
	if (full_path.len() < prefix.len()
	    || memcmp (full_path.mem(), prefix.mem(), prefix.len()))
	{
	    logE_ (_func, "full_path \"", full_path, "\" does not match prefix \"", prefix, "\"");

	    MOMENT_FILE__HEADERS_DATE;
	    ConstMemory const reply_body = "500 Internal Server Error";
	    conn_sender->send (
		    page_pool,
		    MOMENT_FILE__500_HEADERS (reply_body.len()),
		    "\r\n",
		    reply_body);
	    conn_sender->flush ();

	    return Result::Success;
	}

	file_path = full_path.region (prefix.len());
    }

    while (file_path.len() > 0
	   && file_path.mem() [0] == '/')
    {
	file_path = file_path.region (1);
    }

//    logD_ (_func, "file_path: ", file_path);
    if (file_path.len() == 0) {

	file_path = "index.html";

#if 0
	logE_ (_func, "Empty file path\n");

	MOMENT_FILE__HEADERS_DATE;
	ConstMemory const reply_body = "400 Bad Request";
	conn_sender->send (
		page_pool,
		MOMENT_FILE__400_HEADERS (reply_body.len()),
		"\r\n",
		reply_body);
	conn_sender->flush ();

	return Result::Success;
#endif
    }

    ConstMemory mime_type = "text/plain";
    {
	void const * const dot_ptr = memrchr (file_path.mem(), '.', file_path.len());
	if (dot_ptr) {
	    ConstMemory const ext = file_path.region ((Byte const *) (dot_ptr) + 1 - file_path.mem());
	    if (equal (ext, "html"))
		mime_type = "text/html";
	    else
	    if (equal (ext, "css"))
		mime_type = "text/css";
	    else
	    if (equal (ext, "js"))
		mime_type = "application/javascript";
	    else
	    if (equal (ext, "swf"))
		mime_type = "application/x-shockwave-flash";
	    else
	    if (equal (ext, "png"))
		mime_type = "image/png";
	    else
	    if (equal (ext, "jpg"))
		mime_type = "image/jpeg";
	}
    }

    Ref<String> const filename = makeString (path_entry->path->mem(), !path_entry->path->isNull() ? "/" : "", file_path);
//    logD_ (_func, "Opening ", filename);
    NativeFile native_file (filename->mem(),
			    0 /* open_flags */,
			    File::AccessMode::ReadOnly);
    if (exc) {
	logE_ (_func, "Could not open \"", filename, "\": ", exc->toString());

	MOMENT_FILE__HEADERS_DATE;
	ConstMemory const reply_body = "404 Not Found";
	conn_sender->send (
		page_pool,
		MOMENT_FILE__404_HEADERS (reply_body.len()),
		"\r\n",
		reply_body);
	conn_sender->flush ();

	return Result::Success;
    }

    NativeFile::FileStat stat;
    if (!native_file.stat (&stat)) {
	logE_ (_func, "native_file.stat() failed: ", exc->toString());

	MOMENT_FILE__HEADERS_DATE;
	ConstMemory const reply_body = "500 Internal Server Error";
	conn_sender->send (
		page_pool,
		MOMENT_FILE__500_HEADERS (reply_body.len()),
		"\r\n",
		reply_body);
	conn_sender->flush ();

	return Result::Success;
    }

    MOMENT_FILE__HEADERS_DATE;
    conn_sender->send (
	    page_pool,
	    MOMENT_FILE__OK_HEADERS (mime_type, stat.size),
	    "\r\n");

    PagePool::PageListHead page_list;

    Size total_sent = 0;
    Byte buf [65536];
    for (;;) {
	Size num_read;
	IoResult const res = native_file.read (Memory::forObject (buf), &num_read);
	if (res == IoResult::Error) {
	    logE_ (_func, "native_file.read() failed: ", exc->toString());
	    conn_sender->flush ();
	    conn_sender->closeAfterFlush ();
	    return Result::Success;
	}

	// TODO Double copy - not very smart.
	page_pool->getFillPages (&page_list, ConstMemory (buf, num_read));
	total_sent += num_read;

	if (res == IoResult::Eof)
	    break;
    }

    conn_sender->sendPages (page_pool, &page_list);
    conn_sender->flush ();

    assert (total_sent <= stat.size);
    if (total_sent != stat.size) {
	logE_ (_func, "File size mismatch: total_sent: ", total_sent, ", stat.size: ", stat.size);
	conn_sender->closeAfterFlush ();
	return Result::Success;
    }

//    logD_ (_func, "done");
    return Result::Success;
}

HttpService::HttpHandler http_handler = {
    httpRequest,
    NULL /* httpMessageBody */
};

void momentFile_addPath (ConstMemory const &path,
			 ConstMemory const &prefix,
			 HttpService * const http_service)
{
    Ref<PathEntry> const path_entry = grab (new PathEntry);

    path_entry->path = grab (new String (path));
    path_entry->prefix = grab (new String (prefix));

    logD_ (_func, "Adding path \"", path_entry->path, "\", prefix \"", path_entry->prefix->mem(), "\"");

    http_service->addHttpHandler (Cb<HttpService::HttpHandler> (&http_handler, path_entry, NULL /* coderef_container */),
				  path_entry->prefix->mem());

    path_list.append (path_entry);
}

void momentFile_addPathForSection (MConfig::Section * const section,
				   HttpService      * const http_service)
{

    ConstMemory path;
    {
	MConfig::Option * const opt = section->getOption ("path");
	MConfig::Value *val;
	if (opt
	    && (val = opt->getValue()))
	{
	    path = val->mem();
	}
    }

    ConstMemory prefix;
    {
	{
	    MConfig::Option * const opt = section->getOption ("prefix");
	    MConfig::Value *val;
	    if (opt
		&& (val = opt->getValue()))
	    {
		prefix = val->mem();
	    }
	}

	if (prefix.len() > 0
	    && prefix.mem() [0] == '/')
	{
	    prefix = prefix.region (1);
	}
    }

    momentFile_addPath (path, prefix, http_service);
}

// Multiple file paths can be specified like this:
//     mod_file {
//         { path = "/home/user/files"; prefix = "files"; }
//         { path = "/home/user/trash"; prefix = "trash"; }
//     }
//
void momentFileInit ()
{
    logD_ (_func_);

    MomentServer * const moment = MomentServer::getInstance();
//    ServerApp * const server_app = moment->getServerApp();
    MConfig::Config * const config = moment->getConfig();
    HttpService * const http_service = moment->getHttpService();

    page_pool = moment->getPagePool ();

    {
	ConstMemory const opt_name = "mod_file/enable";
	MConfig::Config::BooleanValue const enable = config->getBoolean (opt_name);
	if (enable == MConfig::Config::Boolean_Invalid) {
	    logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
	    return;
	}

	if (enable == MConfig::Config::Boolean_False) {
	    logI_ (_func, "Static HTTP content module (mod_file) is not enabled. "
		   "Set \"", opt_name, "\" option to \"y\" to enable.");
	    return;
	}
    }

    bool got_path = false;
    do {
	MConfig::Section * const modfile_section = config->getSection ("mod_file");
	if (!modfile_section)
	    break;

	MConfig::Section::iter iter (*modfile_section);
	while (!modfile_section->iter_done (iter)) {
	    got_path = true;

	    MConfig::SectionEntry * const sect_entry = modfile_section->iter_next (iter);
	    if (sect_entry->getType() == MConfig::SectionEntry::Type_Section
		&& sect_entry->getName().len() == 0)
	    {
		momentFile_addPathForSection (static_cast <MConfig::Section*> (sect_entry), http_service);
	    }
	}
    } while (0);

    if (!got_path) {
      // Default setup.
	momentFile_addPath ("/opt/moment/myplayer", "moment", http_service);
    }
}

void momentFileUnload ()
{
}

}

}


namespace M {

void libMary_moduleInit ()
{
    Moment::momentFileInit ();
}

void libMary_moduleUnload()
{
    Moment::momentFileUnload ();
}

}

