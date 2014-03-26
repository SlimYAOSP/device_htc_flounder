/*
 * Copyright (C) 2008-2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>

#define LOG_TAG "CwMcuSensor"
#include <cutils/log.h>
#include <cutils/properties.h>
#include <utils/BitSet.h>

#include "CwMcuSensor.h"


#define REL_Significant_Motion REL_WHEEL
#define REL_HTC_Any_Motion 0x09
#define LIGHTSENSOR_LEVEL 10
#define DEBUG_DATA 0
#define COMPASS_CALIBRATION_DATA_SIZE 26
#define NS_PER_MS 1000000LL

/*****************************************************************************/

int fill_block_debug = 0;

pthread_mutex_t sys_fs_mutex = PTHREAD_MUTEX_INITIALIZER;

CwMcuSensor::CwMcuSensor()
    : SensorBase(NULL, "CwMcuSensor")
    , mEnabled(0)
    , mInputReader(4)
    , mHasPendingEventPressure(false)
    , mHasPendingEventActivity(false)
    , mHasPendingEventLight(false)
    , mHasPendingEventMagnetic(false)
    , mHasPendingEventOrientation(false) {
    int rc;

    mPendingEvents[CW_ACCELERATION].version = sizeof(sensors_event_t);
    mPendingEvents[CW_ACCELERATION].sensor = ID_A;
    mPendingEvents[CW_ACCELERATION].type = SENSOR_TYPE_ACCELEROMETER;

    mPendingEvents[CW_MAGNETIC].version = sizeof(sensors_event_t);
    mPendingEvents[CW_MAGNETIC].sensor = ID_M;
    mPendingEvents[CW_MAGNETIC].type = SENSOR_TYPE_MAGNETIC_FIELD;

    mPendingEvents[CW_GYRO].version = sizeof(sensors_event_t);
    mPendingEvents[CW_GYRO].sensor = ID_GY;
    mPendingEvents[CW_GYRO].type = SENSOR_TYPE_GYROSCOPE;

    mPendingEvents[CW_LIGHT].version = sizeof(sensors_event_t);
    mPendingEvents[CW_LIGHT].sensor = ID_L;
    mPendingEvents[CW_LIGHT].type = SENSOR_TYPE_LIGHT;
    memset(mPendingEvents[CW_LIGHT].data, 0, sizeof(mPendingEvents[CW_LIGHT].data));

    mPendingEvents[CW_PRESSURE].version = sizeof(sensors_event_t);
    mPendingEvents[CW_PRESSURE].sensor = ID_PS;
    mPendingEvents[CW_PRESSURE].type = SENSOR_TYPE_PRESSURE;
    memset(mPendingEvents[CW_PRESSURE].data, 0, sizeof(mPendingEvents[CW_PRESSURE].data));

    mPendingEvents[CW_ORIENTATION].version = sizeof(sensors_event_t);
    mPendingEvents[CW_ORIENTATION].sensor = ID_O;
    mPendingEvents[CW_ORIENTATION].type = SENSOR_TYPE_ORIENTATION;
    mPendingEvents[CW_ORIENTATION].orientation.status = SENSOR_STATUS_ACCURACY_HIGH;

    mPendingEvents[CW_ROTATIONVECTOR].version = sizeof(sensors_event_t);
    mPendingEvents[CW_ROTATIONVECTOR].sensor = ID_RV;
    mPendingEvents[CW_ROTATIONVECTOR].type = SENSOR_TYPE_ROTATION_VECTOR;
    mPendingEvents[CW_ROTATIONVECTOR].orientation.status = SENSOR_STATUS_ACCURACY_HIGH;

    mPendingEvents[CW_LINEARACCELERATION].version = sizeof(sensors_event_t);
    mPendingEvents[CW_LINEARACCELERATION].sensor = ID_LA;
    mPendingEvents[CW_LINEARACCELERATION].type = SENSOR_TYPE_LINEAR_ACCELERATION;
    mPendingEvents[CW_LINEARACCELERATION].orientation.status = SENSOR_STATUS_ACCURACY_HIGH;

    mPendingEvents[CW_GRAVITY].version = sizeof(sensors_event_t);
    mPendingEvents[CW_GRAVITY].sensor = ID_G;
    mPendingEvents[CW_GRAVITY].type = SENSOR_TYPE_GRAVITY;
    mPendingEvents[CW_GRAVITY].orientation.status = SENSOR_STATUS_ACCURACY_HIGH;

    mPendingEvents[CW_MAGNETIC_UNCALIBRATED].version = sizeof(sensors_event_t);
    mPendingEvents[CW_MAGNETIC_UNCALIBRATED].sensor = ID_CW_MAGNETIC_UNCALIBRATED;
    mPendingEvents[CW_MAGNETIC_UNCALIBRATED].type = SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED;
    mPendingEvents[CW_MAGNETIC_UNCALIBRATED].orientation.status = SENSOR_STATUS_ACCURACY_HIGH;

    mPendingEvents[CW_GYROSCOPE_UNCALIBRATED].version = sizeof(sensors_event_t);
    mPendingEvents[CW_GYROSCOPE_UNCALIBRATED].sensor = ID_CW_GYROSCOPE_UNCALIBRATED;
    mPendingEvents[CW_GYROSCOPE_UNCALIBRATED].type = SENSOR_TYPE_GYROSCOPE_UNCALIBRATED;
    mPendingEvents[CW_GYROSCOPE_UNCALIBRATED].orientation.status = SENSOR_STATUS_ACCURACY_HIGH;

    mPendingEvents[CW_GAME_ROTATION_VECTOR].version = sizeof(sensors_event_t);
    mPendingEvents[CW_GAME_ROTATION_VECTOR].sensor = ID_CW_GAME_ROTATION_VECTOR;
    mPendingEvents[CW_GAME_ROTATION_VECTOR].type = SENSOR_TYPE_GAME_ROTATION_VECTOR;
    mPendingEvents[CW_GAME_ROTATION_VECTOR].orientation.status = SENSOR_STATUS_ACCURACY_HIGH;

    mPendingEvents[CW_GEOMAGNETIC_ROTATION_VECTOR].version = sizeof(sensors_event_t);
    mPendingEvents[CW_GEOMAGNETIC_ROTATION_VECTOR].sensor = ID_CW_GEOMAGNETIC_ROTATION_VECTOR;
    mPendingEvents[CW_GEOMAGNETIC_ROTATION_VECTOR].type = SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR;
    mPendingEvents[CW_GEOMAGNETIC_ROTATION_VECTOR].orientation.status = SENSOR_STATUS_ACCURACY_HIGH;

    mPendingEvents[CW_SIGNIFICANT_MOTION].version = sizeof(sensors_event_t);
    mPendingEvents[CW_SIGNIFICANT_MOTION].sensor = ID_CW_SIGNIFICANT_MOTION;
    mPendingEvents[CW_SIGNIFICANT_MOTION].type = SENSOR_TYPE_SIGNIFICANT_MOTION;
    mPendingEvents[CW_SIGNIFICANT_MOTION].orientation.status = SENSOR_STATUS_ACCURACY_HIGH;

    mPendingEvents[CW_STEP_DETECTOR].version = sizeof(sensors_event_t);
    mPendingEvents[CW_STEP_DETECTOR].sensor = ID_CW_STEP_DETECTOR;
    mPendingEvents[CW_STEP_DETECTOR].type = SENSOR_TYPE_STEP_DETECTOR;
    mPendingEvents[CW_STEP_DETECTOR].orientation.status = SENSOR_STATUS_ACCURACY_HIGH;

    mPendingEvents[CW_STEP_COUNTER].version = sizeof(sensors_event_t);
    mPendingEvents[CW_STEP_COUNTER].sensor = ID_CW_STEP_COUNTER;
    mPendingEvents[CW_STEP_COUNTER].type = SENSOR_TYPE_STEP_COUNTER;

    if (data_fd) {
        ALOGW("%s: 11 Before pthread_mutex_lock()\n", __func__);
        pthread_mutex_lock(&sys_fs_mutex);
        ALOGW("%s: 11 Acquired pthread_mutex_lock()\n", __func__);

        strcpy(input_sysfs_path,"/sys/class/htc_sensorhub/sensor_hub/");
        input_sysfs_path_len = strlen(input_sysfs_path);

        pthread_mutex_unlock(&sys_fs_mutex);

        setEnable(0, 1); // Inside this function call, we use sys_fs_mutex
    }

    int gs_temp_data[255] = {0};
    int compass_temp_data[255] = {0};


    ALOGW("%s: 22 Before pthread_mutex_lock()\n", __func__);
    pthread_mutex_lock(&sys_fs_mutex);
    ALOGW("%s: 22 Acquired pthread_mutex_lock()\n", __func__);

    //Sensor Calibration init . Waiting for firmware ready
    rc = cw_read_calibrator_file(CW_MAGNETIC, SAVE_PATH_MAG, compass_temp_data);
    if (rc == 0) {
        ALOGD("Get compass calibration data from data/misc/ x is %d ,y is %d ,z is %d\n",
              compass_temp_data[0], compass_temp_data[1], compass_temp_data[2]);
        strcpy(&input_sysfs_path[input_sysfs_path_len], "calibrator_data_mag");
        cw_save_calibrator_file(CW_MAGNETIC, input_sysfs_path, compass_temp_data);
    } else {
        ALOGI("Compass calibration data does not exist\n");
    }

    rc = cw_read_calibrator_file(CW_ACCELERATION, SAVE_PATH_ACC, gs_temp_data);
    if (rc == 0) {
        ALOGD("Get g-sensor user calibration data from data/misc/ x is %d ,y is %d ,z is %d\n",
              gs_temp_data[0],gs_temp_data[1],gs_temp_data[2]);
        strcpy(&input_sysfs_path[input_sysfs_path_len], "calibrator_data_acc");
        if(!(gs_temp_data[0] == 0 && gs_temp_data[1] == 0 && gs_temp_data[2] == 0 ))
            cw_save_calibrator_file(CW_ACCELERATION, input_sysfs_path, gs_temp_data);
    } else {
        ALOGI("G-Sensor user calibration data does not exist\n");
    }

    pthread_mutex_unlock(&sys_fs_mutex);

}

CwMcuSensor::~CwMcuSensor() {
    if (mEnabled) {
        setEnable(0, 0);
    }
}

float CwMcuSensor::indexToValue(size_t index) const {
    static const float luxValues[LIGHTSENSOR_LEVEL] = {
        0.0, 10.0, 40.0, 90.0, 160.0,
        225.0, 320.0, 640.0, 1280.0,
        2600.0
    };

    const size_t maxIndex = (LIGHTSENSOR_LEVEL - 1);
    if (index > maxIndex)
        index = maxIndex;
    return luxValues[index];
}

int CwMcuSensor::setInitialState(int id) {
    struct input_absinfo absinfo;
    float temp_value;

    ALOGD("CwMcuSensor::setInitialState\n");

    switch (id) {
    case ID_L:
        if (!ioctl(data_fd, EVIOCGABS(EVENT_TYPE_LIGHT), &absinfo)) {
            // make sure to report an event immediately
            temp_value = absinfo.value;
            mPendingMask |= 1<<CW_LIGHT;
            mPendingEvents[CW_LIGHT].light = indexToValue(temp_value);
            ALOGD("LightSensor::setInitialState: Reporting mPendingEvent.light = %lf\n",
                  mPendingEvents[CW_LIGHT].light);
            mHasPendingEventLight = true;
        } else {
            ALOGW("LightSensor::setInitialState: Cannot get EVENT_TYPE_LIGHT initial value!\n");
        }
        break;
    case ID_PS:
        if (!ioctl(data_fd, EVIOCGABS(ABS_PRESSURE_X), &absinfo)) {
            temp_value = (float)absinfo.value;
            mPendingMask |= 1<<CW_PRESSURE;
            mPendingEvents[CW_PRESSURE].data[0] = temp_value/100;
            ALOGD("CW_PRESSURE::setInitialState: ABS_PRESSURE_X: value = %d\n",
                  absinfo.value);
            mHasPendingEventPressure = true;
        } else {
            mPendingEvents[CW_PRESSURE].data[0] = 0;
            ALOGW("CW_PRESSURE::setInitialState: Cannot get ABS_PRESSURE_X initial value!\n");
        }

        if (!ioctl(data_fd, EVIOCGABS(ABS_PRESSURE_Y), &absinfo)) {
            temp_value = (float)absinfo.value;
            mPendingMask |= 1<<CW_PRESSURE;
            mPendingEvents[CW_PRESSURE].data[1] = temp_value/100;
            ALOGD("CW_PRESSURE::setInitialState: ABS_PRESSURE_Y: value = %d\n",
                  absinfo.value);
            mHasPendingEventPressure = true;
        } else {
            mPendingEvents[CW_PRESSURE].data[1] = 0;
            ALOGW("CW_PRESSURE::setInitialState: Cannot get ABS_PRESSURE_Y initial value!\n");
        }

        if (!ioctl(data_fd, EVIOCGABS(ABS_PRESSURE_Z), &absinfo)) {
            temp_value = (float)absinfo.value;
            mPendingMask |= 1<<CW_PRESSURE;
            mPendingEvents[CW_PRESSURE].data[2] = temp_value/100;
            ALOGD("CW_PRESSURE::setInitialState: ABS_PRESSURE_Z: value = %d\n",
                  absinfo.value);
            mHasPendingEventPressure = true;
        } else {
            mPendingEvents[CW_PRESSURE].data[2] = 0;
            ALOGW("CW_PRESSURE::setInitialState: Cannot get ABS_PRESSURE_Z initial value!\n");
        }

        break;
    case ID_M:
        if (!ioctl(data_fd, EVIOCGABS(ABS_MAG_ACCURACY), &absinfo)) {
            // make sure to report an event immediately
            temp_value = (float)absinfo.value;
            mPendingMask |= 1<<CW_MAGNETIC;
            mPendingEvents[CW_MAGNETIC].magnetic.status = temp_value;
            ALOGD("CW_MAGNETIC::setInitialState: mPendingEvent.status = %d\n",
                  mPendingEvents[CW_MAGNETIC].magnetic.status);
            mHasPendingEventMagnetic = true;
        } else {
            ALOGW("CW_MAGNETIC::setInitialState: strerr = %s\n",
                  strerror(errno));
        }
        break;

    case ID_O:
        if(!ioctl(data_fd, EVIOCGABS(ABS_ORI_ACCURACY), &absinfo)) {
            // make sure to report an event immediately
            temp_value = (float)absinfo.value;
            mPendingEvents[CW_ORIENTATION].orientation.status = temp_value;
            ALOGD("CW_ORIENTATION::setInitialState: mPendingEvent.status = %d\n",
                  mPendingEvents[CW_ORIENTATION].orientation.status);
        } else {
            ALOGW("CW_ORIENTATION::setInitialState: strerr = %s\n",
                  strerror(errno));
        }
        break;

    default:
        ALOGW("AkmSensor::setInitialState: unknown id (%d)", id);
        return -EINVAL;
    }

    return 0;
}


int CwMcuSensor::find_sensor(int32_t handle) {
    int what = -1;
    switch (handle) {
    case ID_A:
        what = CW_ACCELERATION;
        break;
    case ID_M:
        what = CW_MAGNETIC;
        break;
    case ID_GY:
        what = CW_GYRO;
        break;
    case ID_PS:
        what = CW_PRESSURE;
        break;
    case ID_O:
        what = CW_ORIENTATION;
        break;
    case ID_RV:
        what = CW_ROTATIONVECTOR;
        break;
    case ID_LA:
        what = CW_LINEARACCELERATION;
        break;
    case ID_G:
        what = CW_GRAVITY;
        break;
    case ID_CW_MAGNETIC_UNCALIBRATED:
        what = CW_MAGNETIC_UNCALIBRATED;
        break;
    case ID_CW_GYROSCOPE_UNCALIBRATED:
        what = CW_GYROSCOPE_UNCALIBRATED;
        break;
    case ID_CW_GAME_ROTATION_VECTOR:
        what = CW_GAME_ROTATION_VECTOR;
        break;
    case ID_CW_GEOMAGNETIC_ROTATION_VECTOR:
        what = CW_GEOMAGNETIC_ROTATION_VECTOR;
        break;
    case ID_CW_SIGNIFICANT_MOTION:
        what = CW_SIGNIFICANT_MOTION;
        break;
    case ID_CW_STEP_DETECTOR:
        what = CW_STEP_DETECTOR;
        break;
    case ID_CW_STEP_COUNTER:
        what = CW_STEP_COUNTER;
        break;
    case ID_L:
        what = CW_LIGHT;
        break;
    case ID_Any_Motion:
        what = HTC_ANY_MOTION;
        break;
    }
    return what;
}

int CwMcuSensor::getEnable(int32_t handle) {
    ALOGD("CwMcuSensor::getEnable: handle = %d\n", handle);
    return  0;
}

int CwMcuSensor::setEnable(int32_t handle, int en) {

    int what;
    int err = 0;
    int flags = !!en;
    int fd;
    char buf[10];
    int temp_data[COMPASS_CALIBRATION_DATA_SIZE];
    char value[PROPERTY_VALUE_MAX] = {0};
    int rc;

    ALOGW("%s: Before pthread_mutex_lock()\n", __func__);
    pthread_mutex_lock(&sys_fs_mutex);
    ALOGW("%s: Acquired pthread_mutex_lock()\n", __func__);

    property_get("debug.sensorhal.fill.block", value, "0");
    ALOGD("CwMcuSensor::setEnable: debug.sensorhal.fill.block= %s", value);
    if (atoi(value) == 1)
        fill_block_debug = 1;
    else
        fill_block_debug = 0;

    what = find_sensor(handle);

    ALOGD("CwMcuSensor::setEnable: [v01-initial version], handle = %d, en = %d, what = %d\n", handle, en, what);

    if (uint32_t(what) >= numSensors) {
        pthread_mutex_unlock(&sys_fs_mutex);
        return -EINVAL;
    }
    strcpy(&input_sysfs_path[input_sysfs_path_len], "enable");
    fd = open(input_sysfs_path, O_RDWR);
    if (fd >= 0) {
        snprintf(buf, sizeof(buf), "%d %d\n", what, flags);
        err = write(fd, buf, strlen(buf));
        if (err < 0) {
            ALOGE("%s: write failed: %s", __func__, strerror(errno));
        }

        close(fd);

        if ((what == CW_MAGNETIC_UNCALIBRATED) ||
                (what == CW_PRESSURE) ||
                (what == CW_LIGHT) ||
                (what == CW_MAGNETIC) ||
                (what == CW_ORIENTATION)
           ) {
            if (en)
                setInitialState(what);
        }

        mEnabled &= ~(1<<what);
        mEnabled |= (uint32_t(flags)<<what);
    } else {
        ALOGE("%s open failed: %s", __func__, strerror(errno));
    }

    // Sensor Calibration init. Waiting for firmware ready
    if (((what == CW_MAGNETIC) && (flags == 0)) ||
            ((what == CW_ORIENTATION) && (flags == 0)) ||
            ((what == CW_ROTATIONVECTOR) && (flags == 0))
       ) {
        ALOGD("Save Compass calibration data");
        strcpy(&input_sysfs_path[input_sysfs_path_len], "calibrator_data_mag");
        rc = cw_read_calibrator_file(CW_MAGNETIC, input_sysfs_path, temp_data);
        if (rc== 0) {
            cw_save_calibrator_file(CW_MAGNETIC, SAVE_PATH_MAG, temp_data);
        } else
            ALOGI("Compass calibration data from driver fails\n");
    }

    pthread_mutex_unlock(&sys_fs_mutex);
    return 0;
}

bool CwMcuSensor::hasPendingEvents() const {
    ALOGD_IF(fill_block_debug == 1, "CwMcuSensor::hasPendingEvents: "
             "mHasPendingEvent(Pre, Act, Lig, Mag, Ori) = (%d, %d, %d, %d, %d, %d)\n",
             mHasPendingEventPressure,
             mHasPendingEventActivity,
             mHasPendingEventLight,
             mHasPendingEventMagnetic,
             mHasPendingEventOrientation
            );
    return (mHasPendingEventPressure ||
            mHasPendingEventActivity ||
            mHasPendingEventLight ||
            mHasPendingEventMagnetic ||
            mHasPendingEventOrientation
           );
}

int CwMcuSensor::setDelay(int32_t handle, int64_t delay_ns) {
    char buf[80];
    int fd;
    int what;
    int rc;

    ALOGW("%s: Before pthread_mutex_lock()\n", __func__);
    pthread_mutex_lock(&sys_fs_mutex);
    ALOGW("%s: Acquired pthread_mutex_lock()\n", __func__);

    ALOGD("CwMcuSensor::setDelay: handle = %d, delay_ns = %lld\n", handle, delay_ns);

    what = find_sensor(handle);
    if (uint32_t(what) >= numSensors) {
        pthread_mutex_unlock(&sys_fs_mutex);
        return -EINVAL;
    }
    strcpy(&input_sysfs_path[input_sysfs_path_len], "delay_ms");
    fd = open(input_sysfs_path, O_RDWR);
    if (fd >= 0) {
        snprintf(buf, sizeof(buf), "%d %lld\n", what, (delay_ns/NS_PER_MS));
        write(fd, buf, strlen(buf));
        close(fd);
    }

    pthread_mutex_unlock(&sys_fs_mutex);
    return 0;

}

void CwMcuSensor::calculate_rv_4th_element(int sensors_id) {
    switch (sensors_id) {
    case CW_ROTATIONVECTOR:
    case CW_GAME_ROTATION_VECTOR:
    case CW_GEOMAGNETIC_ROTATION_VECTOR:
        float q0, q1, q2, q3;

        q1 = mPendingEvents[sensors_id].data[0];
        q2 = mPendingEvents[sensors_id].data[1];
        q3 = mPendingEvents[sensors_id].data[2];

        q0 = 1 - q1*q1 - q2*q2 - q3*q3;
        q0 = (q0 > 0) ? (float)sqrt(q0) : 0;

        mPendingEvents[sensors_id].data[3] = q0;
        break;
    default:
        break;
    }
}

int CwMcuSensor::readEvents(sensors_event_t* data, int count) {
    int64_t mtimestamp = getTimestamp();

    if (count < 1)
        return -EINVAL;

    if(mHasPendingEventPressure) {
        ALOGD("CwMcuSensor::readEvents: Init pressure\n");
        mHasPendingEventPressure = false;
        mPendingEvents[CW_PRESSURE].timestamp = mtimestamp;
        *data = mPendingEvents[CW_PRESSURE];
        ALOGD("CwMcuSensor::readEvents: returning pressure, mEnabled = 0x%x\n", mEnabled);
        return !!mEnabled;
    }

    if(mHasPendingEventLight) {
        ALOGD("CwMcuSensor::readEvents: Init light\n");
        mHasPendingEventLight = false;
        mPendingEvents[CW_LIGHT].timestamp = mtimestamp;
        *data = mPendingEvents[CW_LIGHT];
        ALOGD("CwMcuSensor::readEvents: returning light, mEnabled = 0x%x\n", mEnabled);
        return !!mEnabled;
    }

    if (mHasPendingEventMagnetic) {
        ALOGD("CwMcuSensor::readEvents: Init CW_MAGNETIC\n");
        mHasPendingEventMagnetic = false;
        mPendingEvents[CW_MAGNETIC].timestamp = mtimestamp;
        *data = mPendingEvents[CW_MAGNETIC];
        ALOGD("CwMcuSensor::readEvents: returning CW_MAGNETIC, mEnabled = 0x%x\n", mEnabled);
        return !!mEnabled;
    }

    if (mHasPendingEventOrientation) {
        ALOGD("CwMcuSensor::readEvents: Init CW_ORIENTATION\n");
        mHasPendingEventOrientation = false;
        mPendingEvents[CW_ORIENTATION].timestamp = mtimestamp;
        *data = mPendingEvents[CW_ORIENTATION];
        ALOGD("CwMcuSensor::readEvents: reporting CW_ORIENTATION(y, p, r) ="
              " (%8.5f, %8.5f, %8.5f), mEnabled = 0x%x\n",
              mPendingEvents[CW_ORIENTATION].data[0],
              mPendingEvents[CW_ORIENTATION].data[1],
              mPendingEvents[CW_ORIENTATION].data[2],
              mEnabled);
        ALOGD("CwMcuSensor::readEvents: returning CW_ORIENTATION, mEnabled = 0x%x\n", mEnabled);
        return !!mEnabled;
    }

    ALOGD_IF(fill_block_debug == 1, "CwMcuSensor::readEvents: Before fill\n");
    ssize_t n = mInputReader.fill(data_fd);
    ALOGD_IF(fill_block_debug == 1, "CwMcuSensor::readEvents: After fill, n = %d\n", n);
    if (n < 0)
        return n;

    int numEventReceived = 0;
    input_event const* event;
    while (count && mInputReader.readEvent(&event)) {

        if (event->type == EV_ABS) {
            if(event->code == EVENT_TYPE_LIGHT) {
                if (event->value != -1) {
                    mPendingMask |= 1<<CW_LIGHT;
                    mPendingEvents[CW_LIGHT].light = indexToValue(event->value);
                    ALOGD("HUB LightSensor::readEvents: Reporting mPendingEvent.light = %f\n", mPendingEvents[CW_LIGHT].light);
                }
            } else {
                processEvent(event->code, event->value);
            }

        } else if (event->type == EV_REL) {
            ALOGD("CwMcuSensor ==REL==: event (type=%d, code=%d, value=%d)\n",
                  event->type, event->code, event->value);

            if(event->code == REL_HTC_Any_Motion) {
                mPendingMask |= 1<<HTC_ANY_MOTION;
                mPendingEvents[HTC_ANY_MOTION].data[0] = 1;
                ALOGI("HTC_ANY_MOTION\n");
            }

            if(event->code == REL_Significant_Motion) {
                mPendingMask |= 1<<CW_SIGNIFICANT_MOTION;
                mPendingEvents[CW_SIGNIFICANT_MOTION].data[0] = 1;
                ALOGI("Significant Motion detected!\n");
            }
        } else if (event->type == EV_SYN) {
            int64_t time = timevalToNano(event->time);
            for (int j=0 ; count && mPendingMask && j<numSensors ; j++) {
                if (mPendingMask & (1<<j)) {
                    mPendingMask &= ~(1<<j);

                    if (j == CW_SIGNIFICANT_MOTION)
                        setEnable(ID_CW_SIGNIFICANT_MOTION, 0);

                    mPendingEvents[j].timestamp = time;
                    if (mEnabled & (1<<j)) {

                        calculate_rv_4th_element(j);

                        *data++ = mPendingEvents[j];
                        count--;
                        numEventReceived++;
                    }
                }
            }
        } else {
            ALOGE("CwMcuSensor: unknown event (type=%d, code=%d)",
                  event->type, event->code);
        }
        mInputReader.next();
    }
    return numEventReceived;
}

void CwMcuSensor::processEvent(int code, float value) {
    switch (code) {
    case ABS_ACC_X:
        mPendingMask |= 1<<CW_ACCELERATION;
        mPendingEvents[CW_ACCELERATION].data[0] = value * CONVERT_A;
        break;
    case ABS_ACC_Y:
        mPendingMask |= 1<<CW_ACCELERATION;
        mPendingEvents[CW_ACCELERATION].data[1] = value * CONVERT_A;
        break;
    case ABS_ACC_Z:
        mPendingMask |= 1<<CW_ACCELERATION;
        mPendingEvents[CW_ACCELERATION].data[2] = value * CONVERT_A;
        break;

    case ABS_MAG_X:
        mPendingMask |= 1<<CW_MAGNETIC;
        mPendingEvents[CW_MAGNETIC].data[0] = value * CONVERT_M;
        break;
    case ABS_MAG_Y:
        mPendingMask |= 1<<CW_MAGNETIC;
        mPendingEvents[CW_MAGNETIC].data[1] = value * CONVERT_M;
        break;
    case ABS_MAG_Z:
        mPendingMask |= 1<<CW_MAGNETIC;
        mPendingEvents[CW_MAGNETIC].data[2] = value * CONVERT_M;
        break;
    case ABS_MAG_ACCURACY:
        mPendingMask |= 1<<CW_MAGNETIC;
        ALOGD("ABS_MAG_ACCURACY: value = %f\n", value);
        mPendingEvents[CW_MAGNETIC].magnetic.status = value;
        break;

    case ABS_GYRO_X:
        mPendingMask |= 1<<CW_GYRO;
        mPendingEvents[CW_GYRO].data[0] = value * CONVERT_GYRO;
        break;
    case ABS_GYRO_Y:
        mPendingMask |= 1<<CW_GYRO;
        mPendingEvents[CW_GYRO].data[1] = value * CONVERT_GYRO;
        break;
    case ABS_GYRO_Z:
        mPendingMask |= 1<<CW_GYRO;
        mPendingEvents[CW_GYRO].data[2] = value * CONVERT_GYRO;
        break;
    case ABS_PRESSURE_X:
        mPendingMask |= 1<<CW_PRESSURE;
        mPendingEvents[CW_PRESSURE].data[0] = value/CONVERT_PRESSURE;
        break;
    case ABS_PRESSURE_Y:
        mPendingMask |= 1<<CW_PRESSURE;
        mPendingEvents[CW_PRESSURE].data[1] = value/CONVERT_PRESSURE;
        break;
    case ABS_PRESSURE_Z:
        mPendingMask |= 1<<CW_PRESSURE;
        mPendingEvents[CW_PRESSURE].data[2] = value/CONVERT_PRESSURE;
        break;
    case ABS_ORI_X:
        mPendingMask |= 1<<CW_ORIENTATION;
        mPendingEvents[CW_ORIENTATION].data[0] = value * CONVERT_O;
        ALOGD_IF((DEBUG_DATA == 1), "CW_ORIENTATION [0] = %f\n", mPendingEvents[CW_ORIENTATION].data[0]);
        break;
    case ABS_ORI_Y:
        mPendingMask |= 1<<CW_ORIENTATION;
        mPendingEvents[CW_ORIENTATION].data[1] = value * CONVERT_O;
        ALOGD_IF((DEBUG_DATA == 1), "CW_ORIENTATION [1] = %f\n", mPendingEvents[CW_ORIENTATION].data[1]);
        break;
    case ABS_ORI_Z:
        mPendingMask |= 1<<CW_ORIENTATION;
        mPendingEvents[CW_ORIENTATION].data[2] = value * CONVERT_O;
        ALOGD_IF((DEBUG_DATA == 1), "CW_ORIENTATION [2] = %f\n", mPendingEvents[CW_ORIENTATION].data[2]);
        break;
    case ABS_ORI_ACCURACY:
        mPendingMask |= 1<<CW_ORIENTATION;
        ALOGD("ABS_ORI_ACCURACY: value = %f\n", value);
        mPendingEvents[CW_ORIENTATION].orientation.status = value;
        break;

    case ABS_ROT_X:
        mPendingMask |= 1<<CW_ROTATIONVECTOR;
        mPendingEvents[CW_ROTATIONVECTOR].data[0] = value/CONVERT_RV;
        break;
    case ABS_ROT_Y:
        mPendingMask |= 1<<CW_ROTATIONVECTOR;
        mPendingEvents[CW_ROTATIONVECTOR].data[1] = value/CONVERT_RV;
        break;
    case ABS_ROT_Z:
        mPendingMask |= 1<<CW_ROTATIONVECTOR;
        mPendingEvents[CW_ROTATIONVECTOR].data[2] = value/CONVERT_RV;
        break;

    case ABS_LIN_X:
        mPendingMask |= 1<<CW_LINEARACCELERATION;
        mPendingEvents[CW_LINEARACCELERATION].data[0] = value * CONVERT_ALL;
        break;
    case ABS_LIN_Y:
        mPendingMask |= 1<<CW_LINEARACCELERATION;
        mPendingEvents[CW_LINEARACCELERATION].data[1] = value * CONVERT_ALL;
        break;
    case ABS_LIN_Z:
        mPendingMask |= 1<<CW_LINEARACCELERATION;
        mPendingEvents[CW_LINEARACCELERATION].data[2] = value * CONVERT_ALL;
        break;

    case ABS_GRA_X:
        mPendingMask |= 1<<CW_GRAVITY;
        mPendingEvents[CW_GRAVITY].data[0] = value * CONVERT_ALL;
        break;
    case ABS_GRA_Y:
        mPendingMask |= 1<<CW_GRAVITY;
        mPendingEvents[CW_GRAVITY].data[1] = value * CONVERT_ALL;
        break;
    case ABS_GRA_Z:
        mPendingMask |= 1<<CW_GRAVITY;
        mPendingEvents[CW_GRAVITY].data[2] = value * CONVERT_ALL;
        break;

    case ABS_MAGNETIC_UNCALIBRATED_X:
        mPendingMask |= 1<<CW_MAGNETIC_UNCALIBRATED;
        mPendingEvents[CW_MAGNETIC_UNCALIBRATED].uncalibrated_magnetic.uncalib[0] = value * CONVERT_M;
        break;
    case ABS_MAGNETIC_UNCALIBRATED_Y:
        mPendingMask |= 1<<CW_MAGNETIC_UNCALIBRATED;
        mPendingEvents[CW_MAGNETIC_UNCALIBRATED].uncalibrated_magnetic.uncalib[1] = value * CONVERT_M;
        break;
    case ABS_MAGNETIC_UNCALIBRATED_Z:
        mPendingMask |= 1<<CW_MAGNETIC_UNCALIBRATED;
        mPendingEvents[CW_MAGNETIC_UNCALIBRATED].uncalibrated_magnetic.uncalib[2] = value * CONVERT_M;
        break;
    case ABS_MAGNETIC_UNCALIBRATED_BIAS_X:
        mPendingMask |= 1<<CW_MAGNETIC_UNCALIBRATED;
        mPendingEvents[CW_MAGNETIC_UNCALIBRATED].uncalibrated_magnetic.bias[0] = value * CONVERT_M;
        break;
    case ABS_MAGNETIC_UNCALIBRATED_BIAS_Y:
        mPendingMask |= 1<<CW_MAGNETIC_UNCALIBRATED;
        mPendingEvents[CW_MAGNETIC_UNCALIBRATED].uncalibrated_magnetic.bias[1] = value * CONVERT_M;
        break;
    case ABS_MAGNETIC_UNCALIBRATED_BIAS_Z:
        mPendingMask |= 1<<CW_MAGNETIC_UNCALIBRATED;
        mPendingEvents[CW_MAGNETIC_UNCALIBRATED].uncalibrated_magnetic.bias[2] = value * CONVERT_M;
        break;

    case ABS_GYROSCOPE_UNCALIBRATED_X:
        mPendingMask |= 1<<CW_GYROSCOPE_UNCALIBRATED;
        mPendingEvents[CW_GYROSCOPE_UNCALIBRATED].uncalibrated_gyro.uncalib[0] = value * CONVERT_GYRO;
        break;
    case ABS_GYROSCOPE_UNCALIBRATED_Y:
        mPendingMask |= 1<<CW_GYROSCOPE_UNCALIBRATED;
        mPendingEvents[CW_GYROSCOPE_UNCALIBRATED].uncalibrated_gyro.uncalib[1] = value * CONVERT_GYRO;
        break;
    case ABS_GYROSCOPE_UNCALIBRATED_Z:
        mPendingMask |= 1<<CW_GYROSCOPE_UNCALIBRATED;
        mPendingEvents[CW_GYROSCOPE_UNCALIBRATED].uncalibrated_gyro.uncalib[2] = value * CONVERT_GYRO;
        break;
    case ABS_GYROSCOPE_UNCALIBRATED_BIAS_X:
        mPendingMask |= 1<<CW_GYROSCOPE_UNCALIBRATED;
        mPendingEvents[CW_GYROSCOPE_UNCALIBRATED].uncalibrated_gyro.bias[0] = value * CONVERT_GYRO;
        break;
    case ABS_GYROSCOPE_UNCALIBRATED_BIAS_Y:
        mPendingMask |= 1<<CW_GYROSCOPE_UNCALIBRATED;
        mPendingEvents[CW_GYROSCOPE_UNCALIBRATED].uncalibrated_gyro.bias[1] = value * CONVERT_GYRO;
        break;
    case ABS_GYROSCOPE_UNCALIBRATED_BIAS_Z:
        mPendingMask |= 1<<CW_GYROSCOPE_UNCALIBRATED;
        mPendingEvents[CW_GYROSCOPE_UNCALIBRATED].uncalibrated_gyro.bias[2] = value * CONVERT_GYRO;
        break;

    case ABS_GAME_ROTATION_VECTOR_X:
        mPendingMask |= 1<<CW_GAME_ROTATION_VECTOR;
        mPendingEvents[CW_GAME_ROTATION_VECTOR].data[0] = value / CONVERT_RV;
        break;
    case ABS_GAME_ROTATION_VECTOR_Y:
        mPendingMask |= 1<<CW_GAME_ROTATION_VECTOR;
        mPendingEvents[CW_GAME_ROTATION_VECTOR].data[1] = value / CONVERT_RV;
        break;
    case ABS_GAME_ROTATION_VECTOR_Z:
        mPendingMask |= 1<<CW_GAME_ROTATION_VECTOR;
        mPendingEvents[CW_GAME_ROTATION_VECTOR].data[2] = value / CONVERT_RV;
        break;

    case ABS_GEOMAGNETIC_ROTATION_VECTOR_X:
        mPendingMask |= 1<<CW_GEOMAGNETIC_ROTATION_VECTOR;
        mPendingEvents[CW_GEOMAGNETIC_ROTATION_VECTOR].data[0] = value / CONVERT_RV;
        break;
    case ABS_GEOMAGNETIC_ROTATION_VECTOR_Y:
        mPendingMask |= 1<<CW_GEOMAGNETIC_ROTATION_VECTOR;
        mPendingEvents[CW_GEOMAGNETIC_ROTATION_VECTOR].data[1] = value / CONVERT_RV;
        break;
    case ABS_GEOMAGNETIC_ROTATION_VECTOR_Z:
        mPendingMask |= 1<<CW_GEOMAGNETIC_ROTATION_VECTOR;
        mPendingEvents[CW_GEOMAGNETIC_ROTATION_VECTOR].data[2] = value / CONVERT_RV;
        break;

    case ABS_STEP_DETECTOR:
        mPendingMask |= 1<<CW_STEP_DETECTOR;
        mPendingEvents[CW_STEP_DETECTOR].data[0] = value;
        ALOGD("CW_STEP_DETECTOR: value = %f\n", value);
        break;

    case ABS_STEP_COUNTER:
        mPendingMask |= 1<<CW_STEP_COUNTER;
        mPendingEvents[CW_STEP_COUNTER].data[0] = value;
        ALOGD("CW_STEP_COUNTER: value = %f\n", value);
        break;
    }

}


void CwMcuSensor::cw_save_calibrator_file(int type, const char * path, int* str) {
    FILE *fp_file;
    int i;
    char buf[256] = {0};
    char *pbuf = &buf[0];
    int rc;

    ALOGD("CwMcuSensor::cw_save_calibrator_file: path = %s\n", path);

    fp_file = fopen(path, "w+");
    if (!fp_file) {
        ALOGE("CwMcuSensor::cw_save_calibrator_file: open file '%s' failed: %s\n",
              path, strerror(errno));
        return;
    }

    if ((type == CW_GYRO) || (type == CW_ACCELERATION)) {
        fprintf(fp_file, "%d %d %d\n", str[0], str[1], str[2]);
    } else if(type == CW_MAGNETIC) {
        for (i = 0; (pbuf < (buf + sizeof(buf))) && (i < COMPASS_CALIBRATION_DATA_SIZE); i++) {
            ALOGD("CwMcuSensor::cw_save_calibrator_file: str[%d] = %d\n", i, str[i]);
            rc = snprintf(pbuf, (buf + sizeof(buf) - pbuf), "%d ", str[i]);
            if (rc < 0) {
                ALOGE("CwMcuSensor::cw_save_calibrator_file: snprintf fails, i = %d, "
                      "pbuf = 0x%x, buf = 0x%x, rc = %d\n", i, pbuf, buf, rc);
                return;
            }
            pbuf += rc;
        }
        fprintf(fp_file, "%s\n", buf);
    }

    fclose(fp_file);
    return;
}

int CwMcuSensor::cw_read_calibrator_file(int type, const char * path, int* str) {
    FILE *fp;
    int readBytes;
    int data[COMPASS_CALIBRATION_DATA_SIZE] = {0};
    unsigned int i;
    int my_errno;

    ALOGD("CwMcuSensor::cw_read_calibrator_file: path = %s\n", path);

    fp = fopen(path, "r");
    if (!fp) {
        ALOGE("CwMcuSensor::cw_read_calibrator_file: open file '%s' failed: %s\n",
              path, strerror(errno));
        // errno is reset to 0 before return
        return -1;
    }

    if (type == CW_GYRO || type == CW_ACCELERATION) {
        readBytes = fscanf(fp, "%d %d %d\n", &str[0], &str[1], &str[2]);
        my_errno = errno;
        if (readBytes != 3) {
            ALOGE("CwMcuSensor::cw_read_calibrator_file: fscanf3, readBytes = %d, strerror = %s\n", readBytes, strerror(my_errno));
        }

    } else if (type == CW_MAGNETIC) {
        ALOGD("CwMcuSensor::cw_read_calibrator_file: COMPASS_CALIBRATION_DATA_SIZE = %d\n", COMPASS_CALIBRATION_DATA_SIZE);
        // COMPASS_CALIBRATION_DATA_SIZE is 26
        for (i = 0; i < COMPASS_CALIBRATION_DATA_SIZE; i++) {
            readBytes = fscanf(fp, "%d ", &str[i]);
            my_errno = errno;
            ALOGD("CwMcuSensor::cw_read_calibrator_file: str[%d] = %d\n", i, str[i]);
            if (readBytes < 1) {
                ALOGE("CwMcuSensor::cw_read_calibrator_file: fscanf26, readBytes = %d, strerror = %s\n", readBytes, strerror(my_errno));
                fclose(fp);
                return readBytes;
            }
        }
    }
    fclose(fp);
    return 0;
}