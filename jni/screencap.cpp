#include <jni.h>
//#include "com_android_servicescreencap_ScreenCap.h"

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <binder/ProcessState.h>

#include <gui/SurfaceComposerClient.h>
#include <gui/ISurfaceComposer.h>

#include <ui/PixelFormat.h>

#include <SkImageEncoder.h>
#include <SkBitmap.h>
#include <SkData.h>
#include <SkStream.h>

#include <android/log.h>
#define LOG_TAG "ServiceScreenCap"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

using namespace android;

static uint32_t DEFAULT_DISPLAY_ID = ISurfaceComposer::eDisplayIdMain;


static status_t vinfoToPixelFormat(const fb_var_screeninfo& vinfo,
        uint32_t* bytespp, uint32_t* f)
{

    switch (vinfo.bits_per_pixel) {
        case 16:
            *f = PIXEL_FORMAT_RGB_565;
            *bytespp = 2;
            break;
        case 24:
            *f = PIXEL_FORMAT_RGB_888;
            *bytespp = 3;
            break;
        case 32:
            // TODO: do better decoding of vinfo here
            *f = PIXEL_FORMAT_RGBX_8888;
            *bytespp = 4;
            break;
        default:
            return BAD_VALUE;
    }
    return NO_ERROR;
}

static SkBitmap::Config flinger2skia(PixelFormat f)
{
    switch (f) {
        case PIXEL_FORMAT_RGB_565:
            return SkBitmap::kRGB_565_Config;
        default:
            return SkBitmap::kARGB_8888_Config;
    }
}


/*
 * Class:     com_android_servicescreencap_ScreenCap
 * Method:    currentscreen
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jbyteArray
JNICALL ScreenCap_currentscreen(JNIEnv *env,
		jclass clazz) {

	ProcessState::self()->startThreadPool();

	int32_t displayId = DEFAULT_DISPLAY_ID;

	//const char* fn = env->GetStringUTFChars(jpath,NULL);
	//LOGI("=====jpath:%s \n", fn);

	//if (fn == NULL) {
	//	LOGE("=====path = %s \n =====err: %s \n",fn, strerror(errno));
	//	return 1;
	//}
	//int fd = -1;
	//fd = open(fn, O_WRONLY | O_CREAT | O_TRUNC, 0664);
	//LOGI("=====after open ,fd:%d \n",fd);
	//if (fd == -1) {
	//	LOGE("=====err: %s \n", strerror(errno));
	//	return 2;
	//}

	void const* mapbase = MAP_FAILED;
	ssize_t mapsize = -1;

	void const* base = 0;
	uint32_t w, s, h, f;
	size_t size = 0;

	ScreenshotClient screenshot;
	sp < IBinder > display = SurfaceComposerClient::getBuiltInDisplay(displayId);
	int updateStatus = screenshot.update(display);
	bool isUpdate = updateStatus == NO_ERROR;
	if(!isUpdate){
		LOGE("judge isUpdate err: %s , displayId = %d , updateStatus = %d\n",strerror(errno) , displayId , updateStatus);
	}
	if(display == NULL){
		LOGE("display is null");
	}
	if (display != NULL && isUpdate) {
		base = screenshot.getPixels();
		w = screenshot.getWidth();
		h = screenshot.getHeight();
		s = screenshot.getStride();
		f = screenshot.getFormat();
		size = screenshot.getSize();
	} else {
		const char* fbpath = "/dev/graphics/fb0";
		int fb = open(fbpath, O_RDONLY);
		LOGI("=====read framebuffer, fb:%d \n", fb);
		if (fb >= 0) {
			struct fb_var_screeninfo vinfo;
			if (ioctl(fb, FBIOGET_VSCREENINFO, &vinfo) == 0) {
				uint32_t bytespp;
				if (vinfoToPixelFormat(vinfo, &bytespp, &f) == NO_ERROR) {
					size_t offset = (vinfo.xoffset + vinfo.yoffset * vinfo.xres)
							* bytespp;
					w = vinfo.xres;
					h = vinfo.yres;
					s = vinfo.xres;
					size = w * h * bytespp;
					mapsize = offset + size;
					mapbase = mmap(0, mapsize, PROT_READ, MAP_PRIVATE, fb, 0);
					if (mapbase != MAP_FAILED) {
						base = (void const *) ((char const *) mapbase + offset);
					}
				}
			}
			close(fb);
		}else{
			LOGE("=====fb = %d , err: %s \n",fb, strerror(errno));
			return NULL;
		}
	}

	if (base) {
		SkBitmap b;
		b.setConfig(flinger2skia(f), w, h, s * bytesPerPixel(f));
		b.setPixels((void*) base);
		SkDynamicMemoryWStream stream;
		SkImageEncoder::EncodeStream(&stream, b, SkImageEncoder::kPNG_Type,
				SkImageEncoder::kDefaultQuality);
		SkData* streamData = stream.copyToData();
		//write(fd, streamData->data(), streamData->size());
		if(streamData->data() == NULL){
			LOGE("ScreenCap , SCREEN DATA IS NULL");
			return NULL;
		}
		char* buf = static_cast<char*>(const_cast<void*>(streamData->data()));
		int length = streamData->size();
		jbyteArray array = env->NewByteArray(length);
		env->SetByteArrayRegion(array , 0 , length , reinterpret_cast<jbyte*>(buf));
		streamData->unref();
		return array;
	}
	//close (fd);
	if (mapbase != MAP_FAILED) {
		munmap((void *) mapbase, mapsize);
	}
	return NULL;
}


static JNINativeMethod methods[] = {
		{"currentscreen","()[B",(void*)ScreenCap_currentscreen},
};

static int registerNativeMethods(JNIEnv* env,const char* classname,JNINativeMethod* gMethods,int numMethods ){
	jclass clazz;
	clazz = env->FindClass(classname);
	if(clazz == NULL){
		return JNI_FALSE;
	}
	if(env->RegisterNatives(clazz,gMethods,numMethods) <0 ){
		return JNI_FALSE;
	}
	return JNI_TRUE;
}



static int registerNatives(JNIEnv* env)
{
  if (!registerNativeMethods(env, "com/android/servicescreencap/ScreenCap",
                 methods, sizeof(methods) / sizeof(methods[0]))) {
    return JNI_FALSE;
  }

  return JNI_TRUE;
}


typedef union {
    JNIEnv* env;
    void* venv;
} UnionJNIEnvToVoid;

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    UnionJNIEnvToVoid uenv;
    uenv.venv = NULL;
    jint result = -1;
    JNIEnv* env = NULL;


    if (vm->GetEnv(&uenv.venv, JNI_VERSION_1_6) != JNI_OK) {
    	 return result;
    }
    env = uenv.env;

    if (registerNatives(env) != JNI_TRUE) {
    	 return result;
    }

    result = JNI_VERSION_1_6;

    return result;
}


