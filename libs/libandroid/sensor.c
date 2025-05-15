#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <aio.h>

struct ASensorManager {
	int dummy;
};

struct ASensorEventQueue;
struct ASensorList;
struct ASensor;
struct ALooper;
struct AHardwareBuffer;
struct ASensorEvent;

typedef void * ALooper_callbackFunc;

struct ASensorManager a_sensor_manager;

// --- sensor manager

struct ASensorManager* ASensorManager_getInstance()
{
	return &a_sensor_manager;
}

struct ASensorManager* ASensorManager_getInstanceForPackage(const char* packageName)
{
	return &a_sensor_manager;
}

struct ASensor const* ASensorManager_getDefaultSensor(struct ASensorManager* manager, int type)
{
	return NULL; // no sensor of this type exists
}

int ASensorManager_getSensorList(struct ASensorManager* manager, struct ASensorList* list)
{
	return 0; // the number of sensors - any sane app should see this and stop messing with sensors
}

int ASensorManager_destroyEventQueue(struct ASensorManager* manager, struct ASensorEventQueue* queue)
{
	return 0;
}


int ASensorManager_createSharedMemoryDirectChannel(struct ASensorManager* manager, int fd, size_t size)
{
	return 0;
}


int ASensorManager_createHardwareBufferDirectChannel(struct ASensorManager* manager, struct AHardwareBuffer const * buffer, size_t size)
{
	return 0;
}


void ASensorManager_destroyDirectChannel(struct ASensorManager* manager, int channelId)
{
	return;
}


int ASensorManager_configureDirectReport(struct ASensorManager* manager, struct ASensor const* sensor, int channelId, int rate)
{
	return 0;
}

// --- event queue

struct ASensorEventQueue* ASensorManager_createEventQueue(struct ASensorManager* manager, struct  ALooper* looper, int ident, ALooper_callbackFunc callback, void* data)
{
	return NULL;
}

int ASensorEventQueue_registerSensor(struct ASensorEventQueue* queue, struct ASensor const* sensor, int32_t samplingPeriodUs, int64_t maxBatchReportLatencyUs)
{
	return 0;
}


int ASensorEventQueue_enableSensor(struct ASensorEventQueue* queue, struct ASensor const* sensor)
{
	return 0;
}


int ASensorEventQueue_disableSensor(struct ASensorEventQueue* queue, struct ASensor const* sensor)
{
	return 0;
}


int ASensorEventQueue_setEventRate(struct ASensorEventQueue* queue, struct ASensor const* sensor, int32_t usec)
{
	return 0;
}


int ASensorEventQueue_hasEvents(struct ASensorEventQueue* queue)
{
	return 0;
}


ssize_t ASensorEventQueue_getEvents(struct ASensorEventQueue* queue, struct ASensorEvent* events, size_t count)
{
	return 0;
}

// --- sensor

const char* ASensor_getName(struct ASensor const* sensor)
{
	return "FIXME-ASensor_getName";
}

const char* ASensor_getVendor(struct ASensor const* sensor)
{
	return "FIXME-ASensor_getVendor";
}

int ASensor_getType(struct ASensor const* sensor)
{
	return 0;
}
float ASensor_getResolution(struct ASensor const* sensor)
{
	return 1;
}
int ASensor_getMinDelay(struct ASensor const* sensor)
{
	return 0;
}
int ASensor_getFifoMaxEventCount(struct ASensor const* sensor)
{
	return 0;
}
int ASensor_getFifoReservedEventCount(struct ASensor const* sensor)
{
	return 0;
}
const char* ASensor_getStringType(struct ASensor const* sensor)
{
	return "FIXME-ASensor_getStringType";
}
int ASensor_getReportingMode(struct ASensor const* sensor)
{
	return 0;
}
bool ASensor_isWakeUpSensor(struct ASensor const* sensor)
{
	return 0;
}
bool ASensor_isDirectChannelTypeSupported(struct ASensor const* sensor, int channelType)
{
	return 0;
}
int ASensor_getHighestDirectReportRateLevel(struct ASensor const* sensor)
{
	return 1;
}
