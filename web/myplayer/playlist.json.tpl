[
  [ "video RTMP",  "rtmp://{{ThisRtmpServerAddr}}",   "video" ],
  [ "video RTMPT", "rtmpt://{{ThisRtmptServerAddr}}", "video" ],

  [ "1 - mjpeg", "rtmp://{{ThisRtmpServerAddr}}", "cam_1_mjpeg" ],
  [ "1 - rtsp",  "rtmp://{{ThisRtmpServerAddr}}", "cam_1_rtsp"  ],
  [ "2 - mjpeg", "rtmp://{{ThisRtmpServerAddr}}", "cam_2_mjpeg" ],
  [ "2 - rtsp",  "rtmp://{{ThisRtmpServerAddr}}", "cam_2_rtsp"  ],
  [ "2 - rtsp - rtmpt",  "rtmpt://{{ThisRtmptServerAddr}}", "cam_2_rtsp"  ],
  [ "3 - mjpeg",         "rtmp://{{ThisRtmpServerAddr}}",   "cam_3_mjpeg" ],
  [ "3 - mjpeg - rtmpt", "rtmpt://{{ThisRtmptServerAddr}}", "cam_3_mjpeg" ],
  [ "3 - rtsp",          "rtmp://{{ThisRtmpServerAddr}}",   "cam_3_rtsp"  ],

  [ "CAM1-RTSP",      "rtmpt://{{ThisRtmptServerAddr}}",  "cam_3_rtsp"  ],
  [ "CAM1-MJPG",      "rtmpt://{{ThisRtmptServerAddr}}",  "cam_3_mjpeg" ],
  [ "CAM2-RTSP",      "rtmpt://{{ThisRtmptServerAddr}}",  "cam_2_rtsp"  ],
  [ "CAM2-MJPG",      "rtmpt://{{ThisRtmptServerAddr}}",  "cam_2_mjpeg" ],
  [ "CAM3-MJPG",      "rtmpt://{{ThisRtmptServerAddr}}",  "cam_1_mjpeg" ],
  [ "CAM1-RTSP-RTMP", "rtmp://{{ThisRtmpServerAddr}}", "cam_3_rtsp"  ],
  [ "CAM1-MJPG-RTMP", "rtmp://{{ThisRtmpServerAddr}}", "cam_3_mjpeg" ],
  [ "CAM2-RTSP-RTMP", "rtmp://{{ThisRtmpServerAddr}}", "cam_2_rtsp"  ],
  [ "CAM2-MJPG-RTMP", "rtmp://{{ThisRtmpServerAddr}}", "cam_2_mjpeg" ],
  [ "CAM3-MJPG-RTMP", "rtmp://{{ThisRtmpServerAddr}}", "cam_1_mjpeg" ],
]

