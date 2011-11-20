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


#define MOMENT_FILE__CTEMPLATE


#include <libmary/types.h>
#include <cstring>

#include <mycpp/list.h>
#include <libmary/module_init.h>
#include <moment/libmoment.h>

#ifdef MOMENT_FILE__CTEMPLATE
#include <ctemplate/template.h>
#endif


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

Ref<String> this_http_server_addr;
Ref<String> this_rtmp_server_addr;
Ref<String> this_rtmpt_server_addr;

MyCpp::List< Ref<PathEntry> > path_list;

PagePool *page_pool = NULL;

static Result momentFile_sendTemplate (ConstMemory  filename,
				       Sender      * mt_nonnull sender,
				       ConstMemory  mimet_type);

static Result momentFile_sendMemory (ConstMemory  mem,
				     Sender      * mt_nonnull sender,
				     ConstMemory  mime_type);

Result httpRequest (HttpRequest  * const mt_nonnull req,
		    Sender       * const mt_nonnull conn_sender,
		    Memory const & /* msg_body */,
		    void        ** const mt_nonnull /* ret_msg_data */,
		    void         * const _path_entry)
{
    PathEntry * const path_entry = static_cast <PathEntry*> (_path_entry);

    logD_ (_func, req->getRequestLine());

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
		    true /* do_flush */,
		    MOMENT_FILE__500_HEADERS (reply_body.len()),
		    "\r\n",
		    reply_body);
	    if (!req->getKeepalive())
		conn_sender->closeAfterFlush();

	    logA_ ("file 500 ", req->getClientAddress(), " ", req->getRequestLine());

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
		true /* do_flush */,
		MOMENT_FILE__400_HEADERS (reply_body.len()),
		"\r\n",
		reply_body);
	if (!req->getKeepalive())
	    conn_sender->closeAfterFlush();

	return Result::Success;
#endif
    }

    ConstMemory mime_type = "text/plain";
    bool try_template = false;
    Size ext_length = 0;
    {
	void const * const dot_ptr = memrchr (file_path.mem(), '.', file_path.len());
	if (dot_ptr) {
	    ConstMemory const ext = file_path.region ((Byte const *) (dot_ptr) + 1 - file_path.mem());
	    if (equal (ext, "html")) {
		mime_type = "text/html";
		try_template = true;
		ext_length = 5;
	    } else
	    if (equal (ext, "json")) {
		// application/json doesn't work on client side somewhy.
		// mime_type = "application/json";
		try_template = true;
		ext_length = 0;
	    } else
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
//    logD_ (_func, "try_template: ", try_template);

    Ref<String> const filename = makeString (path_entry->path->mem(), !path_entry->path->isNull() ? "/" : "", file_path);
//    logD_ (_func, "Opening ", filename);
    NativeFile native_file (filename->mem(),
			    0 /* open_flags */,
			    File::AccessMode::ReadOnly);
    if (exc) {
#ifdef MOMENT_FILE__CTEMPLATE
	if (try_template) {
	    if (momentFile_sendTemplate (
			makeString (filename->mem().region (0, filename->mem().len() - ext_length),
				    ".tpl")->mem(),
			conn_sender,
			mime_type))
	    {
		if (!req->getKeepalive())
		    conn_sender->closeAfterFlush();

		logA_ ("file 200 ", req->getClientAddress(), " ", req->getRequestLine());

		return Result::Success;
	    }
	}
#endif

	logE_ (_func, "Could not open \"", filename, "\": ", exc->toString());

	MOMENT_FILE__HEADERS_DATE;
	ConstMemory const reply_body = "404 Not Found";
	conn_sender->send (
		page_pool,
		true /* do_flush */,
		MOMENT_FILE__404_HEADERS (reply_body.len()),
		"\r\n",
		reply_body);
	if (!req->getKeepalive())
	    conn_sender->closeAfterFlush();

	logA_ ("file 404 ", req->getClientAddress(), " ", req->getRequestLine());

	return Result::Success;
    }

    NativeFile::FileStat stat;
    if (!native_file.stat (&stat)) {
	logE_ (_func, "native_file.stat() failed: ", exc->toString());

	MOMENT_FILE__HEADERS_DATE;
	ConstMemory const reply_body = "500 Internal Server Error";
	conn_sender->send (
		page_pool,
		true /* do_flush */,
		MOMENT_FILE__500_HEADERS (reply_body.len()),
		"\r\n",
		reply_body);
	if (!req->getKeepalive())
	    conn_sender->closeAfterFlush();

	logA_ ("file 500 ", req->getClientAddress(), " ", req->getRequestLine());

	return Result::Success;
    }

    MOMENT_FILE__HEADERS_DATE;
    conn_sender->send (
	    page_pool,
	    true /* do_flush */, // TODO No need to flush here?
	    MOMENT_FILE__OK_HEADERS (mime_type, stat.size),
	    "\r\n");

    if (equal (req->getMethod(), "HEAD")) {
	if (!req->getKeepalive())
	    conn_sender->closeAfterFlush();

	logA_ ("file 200 ", req->getClientAddress(), " ", req->getRequestLine());

	return Result::Success;
    }

    PagePool::PageListHead page_list;

    // TODO This doesn't work well with large files (eats too much memory).
    Size total_sent = 0;
    Byte buf [65536];
    for (;;) {
	Size toread = sizeof (buf);
	if (stat.size - total_sent < toread)
	    toread = stat.size - total_sent;

	Size num_read;
	IoResult const res = native_file.read (Memory (buf, toread), &num_read);
	if (res == IoResult::Error) {
	    logE_ (_func, "native_file.read() failed: ", exc->toString());
	    conn_sender->flush ();
	    conn_sender->closeAfterFlush ();
	    return Result::Success;
	}
	assert (num_read <= toread);

	// TODO Double copy - not very smart.
	page_pool->getFillPages (&page_list, ConstMemory (buf, num_read));
	total_sent += num_read;
	assert (total_sent <= stat.size);
	if (total_sent == stat.size)
	    break;

	if (res == IoResult::Eof)
	    break;
    }

    conn_sender->sendPages (page_pool, &page_list, true /* do_flush */);

    assert (total_sent <= stat.size);
    if (total_sent != stat.size) {
	logE_ (_func, "File size mismatch: total_sent: ", total_sent, ", stat.size: ", stat.size);
	conn_sender->closeAfterFlush ();
	logA_ ("file 200 ", req->getRequestLine());
	return Result::Success;
    }

    if (!req->getKeepalive())
	conn_sender->closeAfterFlush();

    logA_ ("file 200 ", req->getClientAddress(), " ", req->getRequestLine());

//    logD_ (_func, "done");
    return Result::Success;
}

#if 0
Result httpRequest (HttpRequest  * const mt_nonnull req,
		    Sender       * const mt_nonnull conn_sender,
		    Memory const & /* msg_body */,
		    void        ** const mt_nonnull /* ret_msg_data */,
		    void         * const _path_entry)
#endif

#ifdef MOMENT_FILE__CTEMPLATE
static Result momentFile_sendTemplate (ConstMemory   const filename,
				       Sender      * const mt_nonnull sender,
				       ConstMemory   const mime_type)
{
    // TODO There should be a better way.
    ctemplate::mutable_default_template_cache()->ReloadAllIfChanged (ctemplate::TemplateCache::LAZY_RELOAD);

    ctemplate::TemplateDictionary dict ("tmpl");
    dict.SetValue ("ThisHttpServerAddr", this_http_server_addr->cstr());
    dict.SetValue ("ThisRtmpServerAddr", this_rtmp_server_addr->cstr());
    dict.SetValue ("ThisRtmptServerAddr", this_rtmpt_server_addr->cstr());
    std::string str;
    if (!ctemplate::ExpandTemplate (grab (new String (filename))->cstr(),
				    ctemplate::DO_NOT_STRIP,
				    &dict,
				    &str))
    {
	logE_ ("could not expand template \"", filename, "\": ", str.c_str());
	return Result::Failure;
    }

//    logD_ ("template \"", filename, "\" expanded: ", str.c_str());
//    logD_ ("template \"", filename, "\" expanded");

    return momentFile_sendMemory (ConstMemory ((Byte const *) str.data(), str.length()), sender, mime_type);
}

static Result momentFile_sendMemory (ConstMemory   const mem,
				     Sender      * const mt_nonnull sender,
				     ConstMemory   const mime_type)
{
    MOMENT_FILE__HEADERS_DATE;
    sender->send (page_pool,
		  false /* do_flush */,
		  MOMENT_FILE__OK_HEADERS (mime_type, mem.len()),
		  "\r\n");

    PagePool::PageListHead page_list;
    page_pool->getFillPages (&page_list, mem);
    // TODO pages of zero length => (behavior - ?)
    sender->sendPages (page_pool, &page_list, true /* do_flush */);

    return Result::Success;
}
#endif

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

    http_service->addHttpHandler (
	    CbDesc<HttpService::HttpHandler> (&http_handler, path_entry, NULL /* coderef_container */),
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
    logI_ (_func, "Initializing mod_file");

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

    {
	ConstMemory const opt_name = "moment/this_http_server_addr";
	ConstMemory const opt_val = config->getString (opt_name);
	logI_ (_func, opt_name, ":  ", opt_val);
	if (!opt_val.isNull()) {
	    this_http_server_addr = grab (new String (opt_val));
	} else {
	    this_http_server_addr = grab (new String ("127.0.0.1:8080"));
	    logI_ (_func, opt_name, " config parameter is not set. "
		   "Defaulting to ", this_http_server_addr);
	}
    }

    {
	ConstMemory const opt_name = "moment/this_rtmp_server_addr";
	ConstMemory const opt_val = config->getString (opt_name);
	logI_ (_func, opt_name, ":  ", opt_val);
	if (!opt_val.isNull()) {
	    this_rtmp_server_addr = grab (new String (opt_val));
	} else {
	    this_rtmp_server_addr = grab (new String ("127.0.0.1:1935"));
	    logI_ (_func, opt_name, " config parameter is not set. "
		   "Defaulting to ", this_rtmp_server_addr);
	}
    }

    {
	ConstMemory const opt_name = "moment/this_rtmpt_server_addr";
	ConstMemory const opt_val = config->getString (opt_name);
	logI_ (_func, opt_name, ": ", opt_val);
	if (!opt_val.isNull()) {
	    this_rtmpt_server_addr = grab (new String (opt_val));
	} else {
	    this_rtmpt_server_addr = grab (new String ("127.0.0.1:8081"));
	    logI_ (_func, opt_name, " config parameter is not set. "
		   "Defaulting to ", this_rtmpt_server_addr);
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
	momentFile_addPath ("/opt/moment/mychat", "mychat", http_service);
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

