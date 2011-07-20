#!/bin/sh

#scp -r -P 2222 err momentvideoorg@momentvideo.org:
#scp -r -P 2222 img momentvideoorg@momentvideo.org:
scp -r -P 2222 *.html *.xml *.xsl *.svg Makefile img momentvideoorg@momentvideo.org:

