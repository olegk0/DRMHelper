CFLAGS ?=  -O0 -g -Wall -fmessage-length=0 -fnon-call-exceptions -I /usr/include/libdrm

#OBJS = drm_helper.o drm_test.o

LIBS = -ldrm

TARGET = drm_test


all:    $(TARGET)

$(TARGET): libdrm_helper.a drm_test.o
	$(CC) -o $(TARGET) drm_test.o $(LIBS) -L. -ldrm_helper

drm_test.o: drm_test.c
	$(CC) -O -c $(CFLAGS) drm_test.c

drm_helper.o: drm_helper.c
	$(CC) -O -c $(CFLAGS) drm_helper.c

libdrm_helper.a: drm_helper.o
	$(AR) rcs libdrm_helper.a drm_helper.o

clean:
	$(RM) -f *.o *.a $(TARGET)
