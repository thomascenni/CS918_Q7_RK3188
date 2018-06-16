H264 demo for encode and decode:

Decode:
./testvpu -i ./h264_320x240_coding_7_for_dec.bin -o ./test.yuv -w 320 -h 240 -coding 7 -vframes 15

Encode
./testvpu -t 2 -i ./vpu_enc_input_yuv420sp_320x240.yuv -o ./test.bin -w 320 -h 240 -coding 7

