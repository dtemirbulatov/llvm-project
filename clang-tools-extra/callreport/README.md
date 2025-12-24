## callreport

callreport is a tool for checking library usage for certain name or path.

Here is a typical usage. For example we would like to know all calls to
avcodec library in our application. we compile our example with following command:

clang    -c -o hw_decode.o hw_decode.c -I/usr/include -I.

Now, we can generate JSON report about calls for this library by this command:

callreport -call-report-sourcepath="avcodec" hw_decode.c

With -call-report-sourcepath flag we can set full path to include or substring of include path.

Here is our report in JSON format:{"ffmpeg-8.0.1/doc/examples/hw_decode.c":{"decode_write":["avcodec_receive_frame","avcodec_send_packet"],
"main":["avcodec_parameters_to_context","avcodec_open2","avcodec_free_context","av_packet_free","avcodec_alloc_context3", .... }}



