# -*- makefile -*-

MARCH=-m64

DBUG=-O3 $(MARCH) -D_FILE_OFFSET_BITS=64
# DBUG=-ggdb $(MARCH) -D_FILE_OFFSET_BITS=64 -DDEBUG=1

EXAMPLES = cow_record test_codec_names

# MCXX=g++
# MCC=gcc
MCXX=clang++
MCC=clang

INCS = muxer.h

CFLAGS = $(DBUG) -fno-diagnostics-color -Wno-deprecated-declarations -Wno-unused-result -Wno-write-strings -c -D_GNU_SOURCE -D_REENTRANT -I. -I/usr/local/include/opencv4 -I/usr/include/cairo -I/usr/local/include/ndi -I/usr/local/include -I/usr/X11R6/include

LD = $(MCC)
LDFLAGS = $(DBUG) -L/usr/local/lib -L/usr/lib -L/usr/X11R6/lib

STDDYN = -lopencv_imgproc -lopencv_videoio -lopencv_core -lopencv_imgcodecs -lopencv_highgui -lavformat -lavcodec -lavutil -lswresample -lswscale -lavfilter -lpulse-simple -lpulse -lcairo -lfltk -lX11 -lXext -lXfixes -lm -lstdc++

all: $(EXAMPLES)

pulse_devices.o: pulse_devices.cpp $(INCS) 
	@echo
	@echo "Compile pulse_devices"
	$(MCXX) -D__STDC_CONSTANT_MACROS $(CFLAGS) pulse_devices.cpp 

muxer.o: muxer.cpp $(INCS) 
	@echo
	@echo "Compile muxer"
	$(MCXX) -D__STDC_CONSTANT_MACROS $(CFLAGS) muxer.cpp 

test_codec_names.o: test_codec_names.cpp $(INCS) 
	@echo
	@echo "Compile test_codec_names"
	$(MCXX) -D__STDC_CONSTANT_MACROS $(CFLAGS) test_codec_names.cpp 

cow_record.o: cow_record.cpp $(INCS) 
	@echo
	@echo "Compile cow_record"
	$(MCXX) -D__STDC_CONSTANT_MACROS $(CFLAGS) cow_record.cpp 

cow_record: cow_record.o muxer.o pulse_devices.o
	@echo
	@echo "Link cow_record"
	$(LD) -o cow_record $(LDFLAGS) cow_record.o muxer.o pulse_devices.o $(STDDYN)

test_codec_names: test_codec_names.o muxer.o
	@echo
	@echo "Link test_codec_names"
	$(LD) -o test_codec_names $(LDFLAGS) test_codec_names.o muxer.o $(STDDYN)
