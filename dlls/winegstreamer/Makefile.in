MODULE    = winegstreamer.dll
UNIXLIB   = winegstreamer.so
IMPORTLIB = winegstreamer
IMPORTS   = strmbase ole32 oleaut32 msdmo msvcrt user32
DELAYIMPORTS = mfplat mf
UNIX_CFLAGS  = $(GSTREAMER_CFLAGS)
UNIX_LIBS    = $(GSTREAMER_LIBS) $(PTHREAD_LIBS)

SOURCES = \
	audio_decoder.c \
	color_convert.c \
	main.c \
	media-converter/audioconv.c \
	media-converter/audioconvbin.c \
	media-converter/fossilize.c \
	media-converter/lib.c \
	media-converter/murmur3.c \
	media-converter/protondemuxer.c \
	media-converter/videoconv.c \
	media_sink.c \
	media_source.c \
	mfplat.c \
	quartz_parser.c \
	quartz_transform.c \
	resampler.c \
	rsrc.rc \
	unixlib.c \
	video_decoder.c \
	video_encoder.c \
	video_processor.c \
	wg_allocator.c \
	wg_format.c \
	wg_media_type.c \
	wg_muxer.c \
	wg_parser.c \
	wg_sample.c \
	wg_task_pool.c \
	wg_transform.c \
	winegstreamer_classes.idl \
	wm_reader.c \
	wma_decoder.c
