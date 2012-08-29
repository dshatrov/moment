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
	"Connection: Keep-Alive\r\n"
//	"Cache-Control: max-age=604800\r\n"
//	"Cache-Control: public\r\n"
//	"Cache-Control: no-cache\r\n"

#define MOMENT_FILE__OK_HEADERS(mime_type, content_length) \
	"HTTP/1.1 200 OK\r\n" \
	MOMENT_FILE__COMMON_HEADERS \
	"Content-Type: ", (mime_type), "\r\n" \
	"Content-Length: ", (content_length), "\r\n"

#define MOMENT_FILE__304_HEADERS \
	"HTTP/1.1 304 Not Modified\r\n" \
	MOMENT_FILE__COMMON_HEADERS

#define MOMENT_FILE__404_HEADERS(content_length) \
	"HTTP/1.1 404 Not Found\r\n" \
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

static Ref<String> this_http_server_addr;
static Ref<String> this_rtmp_server_addr;
static Ref<String> this_rtmpt_server_addr;

static MyCpp::List< Ref<PathEntry> > path_list;

static MomentServer *moment = NULL;
static PagePool *page_pool = NULL;

static Result momentFile_sendTemplate (HttpRequest *http_req,
				       ConstMemory  full_path,
				       ConstMemory  filename,
				       Sender      * mt_nonnull sender,
				       ConstMemory  mime_type);

static Result momentFile_sendMemory (ConstMemory  mem,
				     Sender      * mt_nonnull sender,
				     ConstMemory  mime_type);

Result httpRequest (HttpRequest   * const mt_nonnull req,
		    Sender        * const mt_nonnull conn_sender,
		    Memory const  & /* msg_body */,
		    void         ** const mt_nonnull /* ret_msg_data */,
		    void          * const _path_entry)
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
    logD_ (_func, "file_path: ", file_path);

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
    ConstMemory ext;
    {
#ifdef PLATFORM_WIN32
        // MinGW doesn't have memrchr().
        void const *dot_ptr = NULL;
        {
            unsigned char const * const mem = file_path.mem();
            Size const len = file_path.len();
            for (Size i = len; i > 0; --i) {
                if (mem [i - 1] == '.') {
                    dot_ptr = mem + (i - 1);
                    break;
                }
            }
        }
#else
	void const * const dot_ptr = memrchr (file_path.mem(), '.', file_path.len());
#endif
	if (dot_ptr) {
	    ext = file_path.region ((Byte const *) (dot_ptr) + 1 - file_path.mem());
            if (equal (ext, "ts"))
                mime_type = "video/MP2T";
            else
            if (equal (ext, "m3u8"))
//                mime_type = "application/x-mpegURL";
                mime_type = "application/vnd.apple.mpegurl";
            else
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

    Ref<String> const filename =
            makeString (path_entry->path->mem(), !path_entry->path->isNull() ? "/" : "", file_path);
//    logD_ (_func, "Opening ", filename);
#if 0
// Deprecated form.
    NativeFile native_file (filename->mem(),
			    0 /* open_flags */,
			    File::AccessMode::ReadOnly);
    if (exc) {
#endif
    NativeFile native_file;
    logD_ (_func, "Trying ", filename->mem());
    if (!native_file.open (filename->mem(),
                           0 /* open_flags */,
                           File::AccessMode::ReadOnly))
    {
#ifdef MOMENT_FILE__CTEMPLATE
	if (try_template) {
            logD_ (_func, "Trying .tpl");
	    if (momentFile_sendTemplate (
			req,
			req->getFullPath(),
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

        bool opened = false;
        if (equal (ext, "html")) {
          // TODO Set default language in config etc.

            List<HttpRequest::AcceptedLanguage> langs;
            HttpRequest::parseAcceptLanguage (req->getAcceptLanguage(), &langs);

            {
              // Default language with the lowest priority.
                langs.appendEmpty ();
                HttpRequest::AcceptedLanguage * const alang = &langs.getLast();
                alang->lang = grab (new String ("en"));
                // HTTP allows only positive weights, so '-1.0' is always the lowest.
                alang->weight = -1.0;
            }

            typedef AvlTree< HttpRequest::AcceptedLanguage,
                             MemberExtractor< HttpRequest::AcceptedLanguage const,
                                              double const,
                                              &HttpRequest::AcceptedLanguage::weight >,
                             DirectComparator<double> >
                    AcceptedLanguageTree;

            AcceptedLanguageTree tree;

            logD_ (_func, "accepted languages:");
            {
                List<HttpRequest::AcceptedLanguage>::iter iter (langs);
                while (!langs.iter_done (iter)) {
                    List<HttpRequest::AcceptedLanguage>::Element * const el = langs.iter_next (iter);
                    HttpRequest::AcceptedLanguage * const alang = &el->data;
                    logD_ (alang->lang, ", weight ", alang->weight);

                    tree.add (*alang);
                }
            }

#ifdef MOMENT_FILE__CTEMPLATE
            if (try_template) {
                AcceptedLanguageTree::BottomRightIterator iter (tree);
                while (!iter.done()) {
                    HttpRequest::AcceptedLanguage * const alang = &iter.next ().value;
                    logD_ (_func, "Trying .tpl for language \"", alang->lang, "\"");

                    if (momentFile_sendTemplate (
                                req,
                                req->getFullPath(),
                                makeString (filename->mem().region (0, filename->mem().len() - ext_length),
                                            ".",
                                            alang->lang->mem(),
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
            }
#endif

            {
                AcceptedLanguageTree::BottomRightIterator iter (tree);
                while (!iter.done()) {
                    HttpRequest::AcceptedLanguage * const alang = &iter.next ().value;
                    logD_ (_func, "Trying .html for language \"", alang->lang, "\"");

                    if (native_file.open (makeString (filename->mem().region (0, filename->mem().len() - ext_length),
                                                      ".",
                                                      alang->lang->mem(),
                                                      ".html")->mem(),
                                          0 /* open_flags */,
                                          File::AccessMode::ReadOnly))
                    {
                        opened = true;
                        break;
                    }
                }
            }
        } // if ("html")

        if (!opened) {
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
    }

    bool got_mtime = false;
    struct tm mtime;
// TODO Get file modification time on Win32 + enable "304 Not Modified".
#ifndef PLATFORM_WIN32
    if (native_file.getModificationTime (&mtime))
        got_mtime = true;
    else
        logE_ (_func, "native_file.getModificationTime() failed: ", exc->toString());

    if (got_mtime) {
        bool if_none_match__any = false;
        List<HttpRequest::EntityTag> etags;
        {
            ConstMemory const mem = req->getIfNoneMatch();
            if (!mem.isEmpty())
                HttpRequest::parseEntityTagList (mem, &if_none_match__any, &etags);
        }

        bool send_not_modified = false;
        if (if_none_match__any) {
          // We have already opened the file, so it does exist.
            send_not_modified = true;
        } else {
            bool got_if_modified_since = false;
            struct tm if_modified_since;
            {
                ConstMemory const mem = req->getIfModifiedSince();
                if (!mem.isEmpty()) {
                    if (parseHttpTime (mem, &if_modified_since))
                        got_if_modified_since = true;
                    else
                        logW_ (_func, "Could not parse HTTP time: ", mem);
                }
            }

            if (got_if_modified_since ||
                if_none_match__any    ||
                !etags.isEmpty())
            {
                bool expired = false;
                if (compareTime (&mtime, &if_modified_since) == ComparisonResult::Greater)
                    expired = true;

                bool etag_match = etags.isEmpty();
                {
                    List<HttpRequest::EntityTag>::iter iter (etags);
                    while (!etags.iter_done (iter)) {
                        HttpRequest::EntityTag * const etag = &etags.iter_next (iter)->data;

                        struct tm etag_time;
                        if (parseHttpTime (etag->etag->mem(), &etag_time)) {
                            if (compareTime (&mtime, &etag_time) == ComparisonResult::Equal) {
                                etag_match = true;
                                break;
                            }
                        } else {
                            logW_ (_func, "Could not parse etag time: ", etag->etag->mem());
                        }
                    }
                }

                if (etag_match && !expired)
                    send_not_modified = true;
            }
        }

        if (send_not_modified) {
            MOMENT_FILE__HEADERS_DATE;
            conn_sender->send (
                    page_pool,
                    true /* do_flush */,
                    MOMENT_FILE__304_HEADERS,
                    "\r\n");
            if (!req->getKeepalive())
                conn_sender->closeAfterFlush();

            logA_ ("file 304 ", req->getClientAddress(), " ", req->getRequestLine());

            return Result::Success;
        }
    }
#endif /* PLATFORM_WIN32 */

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

    Byte mtime_buf [timeToString_BufSize];
    Size mtime_len = 0;
    if (got_mtime)
        mtime_len = timeToHttpString (Memory::forObject (mtime_buf), &mtime);

    conn_sender->send (
	    page_pool,
            // TODO No need to flush here? (Remember about HEAD path)
	    true /* do_flush */,
	    MOMENT_FILE__OK_HEADERS (mime_type, stat.size),
            // TODO Send "ETag:" ?
            got_mtime ? "Last-Modified: " : "", 
            got_mtime ? ConstMemory (mtime_buf, mtime_len) : ConstMemory(),
            got_mtime ? "\r\n" : "",
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

#if 0
    {
      // DEBUG

        Count size = 0;
        PagePool::Page *cur_page = page_list.first;
        while (cur_page) {
            size += cur_page->data_len;
            cur_page = cur_page->getNextMsgPage();
        }

        logD_ (_func, "total data length: ", size);
    }
#endif

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

#ifdef MOMENT_FILE__CTEMPLATE
namespace {
    class SendTemplate_PageRequest : public MomentServer::PageRequest
    {
    private:
	mt_const HttpRequest *http_req;
	mt_const ctemplate::TemplateDictionary *dict;

    public:
      mt_iface (MomentServer::PageRequest)

	ConstMemory getParameter (ConstMemory const name)
	{
	    return http_req->getParameter (name);
	}

	IpAddress getClientAddress ()
	{
          return http_req->getClientAddress();
	}

	void addHashVar (ConstMemory const name,
			 ConstMemory const value)
	{
	    logD_ (_func, "name: ", name, ", value: ", value);
	    dict->SetValue (String (name).cstr(), String (value).cstr());
	}

	void showSection (ConstMemory const name)
	{
	    dict->ShowSection (String (name).cstr());
	}

      mt_iface_end

	SendTemplate_PageRequest (HttpRequest * const http_req,
				  ctemplate::TemplateDictionary * const dict)
	    : http_req (http_req),
	      dict (dict)
	{
	}
    };
}

static Result momentFile_sendTemplate (HttpRequest * const http_req,
				       ConstMemory   const full_path,
				       ConstMemory   const filename,
				       Sender      * const mt_nonnull sender,
				       ConstMemory   const mime_type)
{
    // TODO There should be a better way.
    ctemplate::mutable_default_template_cache()->ReloadAllIfChanged (ctemplate::TemplateCache::LAZY_RELOAD);

    ctemplate::TemplateDictionary dict ("tmpl");
    dict.SetValue ("ThisHttpServerAddr", this_http_server_addr->cstr());
    dict.SetValue ("ThisRtmpServerAddr", this_rtmp_server_addr->cstr());
    dict.SetValue ("ThisRtmptServerAddr", this_rtmpt_server_addr->cstr());

#if 0
    {
      // MD5 auth test.

        ConstMemory const client_text = "192.168.0.1 12345";
        Ref<String> const timed_text = makeString (((Uint64) getUnixtime() + 1800) / 3600 /* auth timestamp */,
                                                   " ",
                                                   client_text,
                                                   "password");
        logD_ (_func, "timed_text: ", timed_text->mem());
        unsigned char hash_buf [32];
        getMd5HexAscii (timed_text->mem(), Memory::forObject (hash_buf));
        Ref<String> const auth_str = makeString (client_text, "|", Memory::forObject (hash_buf));
        dict.SetValue ("MomentAuthTest", auth_str->cstr());
    }
#endif

    {
	SendTemplate_PageRequest page_req (http_req, &dict);
	MomentServer::PageRequestResult const res = moment->processPageRequest (&page_req, full_path);
	// TODO Check 'res' and react.
    }

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

static HttpService::HttpHandler const http_handler = {
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

    logD_ (_func, "Adding path \"", path_entry->path, "\", prefix \"", path_entry->prefix, "\"");

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

    moment = MomentServer::getInstance();
//    ServerApp * const server_app = moment->getServerApp();
    MConfig::Config * const config = moment->getConfig();
    HttpService * const http_service = moment->getHttpService();

    page_pool = moment->getPagePool ();

    {
	ConstMemory const opt_name = "mod_file/enable";
	MConfig::BooleanValue const enable = config->getBoolean (opt_name);
	if (enable == MConfig::Boolean_Invalid) {
	    logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
	    return;
	}

	if (enable == MConfig::Boolean_False) {
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

} // namespace {}

} // namespace Moment


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

