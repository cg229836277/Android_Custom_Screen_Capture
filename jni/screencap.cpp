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
static int32_t displayId = DEFAULT_DISPLAY_ID;

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
JNICALL ScreenCap_currentscreen(JNIEnv *env,jclass clazz , jint width , jint height) {
	ProcessState::self()->startThreadPool();

	void const* base = 0;

	ScreenshotClient screenshot;
	sp < IBinder > display = SurfaceComposerClient::getBuiltInDisplay(displayId);
	if(display == NULL){
		LOGE("display is null");
		return NULL;
	}
	int updateStatus = screenshot.update(display , width , height);
	bool isUpdate = updateStatus == NO_ERROR;
	if(!isUpdate){
		LOGE("judge isUpdate err: %s , displayId = %d , updateStatus = %d\n",strerror(errno) , displayId , updateStatus);
		return NULL;
	}

	base = screenshot.getPixels();

	if (base) {
		char* buf = static_cast<char*>(const_cast<void*>(base));
		int length = screenshot.getSize();
		jbyteArray array = env->NewByteArray(length);
		env->SetByteArrayRegion(array , 0 , length , reinterpret_cast<jbyte*>(buf));
		return array;
	}
	return NULL;
}


static JNINativeMethod methods[] = {
		{"currentscreen","(II)[B",(void*)ScreenCap_currentscreen},
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


