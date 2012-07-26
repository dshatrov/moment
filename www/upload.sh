#!/bin/sh

# Note that scp doens't work correctly if the new file is smaller than the older one.

#scp -r -P 2222 err momentvideoorg@momentvideo.org:
#scp -r -P 2222 img momentvideoorg@momentvideo.org:
scp -r *.html *.php *.xml *.xsl *.svg *.ico Makefile img root@78.46.194.123:/usr/local/nginx/html/
#scp -P 2222 index.php test.html momentvideoorg@momentvideo.org:

