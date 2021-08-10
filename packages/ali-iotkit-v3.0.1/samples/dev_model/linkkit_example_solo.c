/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 * 
 * Again edit by rt-thread group
 * Change Logs:
 * Date          Author          Notes
 * 2019-07-21    MurphyZhao      first edit
 */

#include "rtthread.h"
#include "infra_config.h"

extern void HAL_Printf(const char *fmt, ...);
extern int HAL_Snprintf(char *str, const int len, const char *fmt, ...);

#ifdef DEPRECATED_LINKKIT
#include "solo.c"
#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "infra_types.h"
#include "infra_defs.h"
#include "infra_compat.h"
#include "infra_compat.h"
#ifdef INFRA_MEM_STATS
#include "infra_mem_stats.h"
#endif
#include "dev_model_api.h"
#include "dm_wrapper.h"
#include "cJSON.h"
#ifdef ATM_ENABLED
#include "at_api.h"
#endif

#define EXAMPLE_TRACE(...)                                      \
    do                                                          \
    {                                                           \
        HAL_Printf("\033[1;32;40m%s.%d: ", __func__, __LINE__); \
        HAL_Printf(__VA_ARGS__);                                \
        HAL_Printf("\033[0m\r\n");                              \
    } while (0)

#define EXAMPLE_MASTER_DEVID (0)
#define EXAMPLE_YIELD_TIMEOUT_MS (200)

typedef struct
{
    int master_devid;
    int cloud_connected;
    int master_initialized;
} user_example_ctx_t;


#include <rtdevice.h>
#include "board.h"

#define OPEN_PIN GET_PIN(E,1)
#define CLOSE_PIN GET_PIN(E,2)
#define STOP_PIN GET_PIN(E,3)
#define LOCK_PIN GET_PIN(E,4)

/**
 * These PRODUCT_KEY|PRODUCT_SECRET|DEVICE_NAME|DEVICE_SECRET are listed for demo only
 *
 * When you created your own devices on iot.console.com, you SHOULD replace them with what you got from console
 *
 */

char PRODUCT_KEY[IOTX_PRODUCT_KEY_LEN + 1] = {0};
char PRODUCT_SECRET[IOTX_PRODUCT_SECRET_LEN + 1] = {0};
char DEVICE_NAME[IOTX_DEVICE_NAME_LEN + 1] = {0};
char DEVICE_SECRET[IOTX_DEVICE_SECRET_LEN + 1] = {0};

static user_example_ctx_t g_user_example_ctx;

/** cloud connected event callback */
static int user_connected_event_handler(void)
{
    EXAMPLE_TRACE("Cloud Connected");
    g_user_example_ctx.cloud_connected = 1;

    return 0;
}

/** cloud disconnected event callback */
static int user_disconnected_event_handler(void)
{
    EXAMPLE_TRACE("Cloud Disconnected");
    g_user_example_ctx.cloud_connected = 0;

    return 0;
}

/* device initialized event callback */
static int user_initialized(const int devid)
{
    EXAMPLE_TRACE("Device Initialized");
    g_user_example_ctx.master_initialized = 1;

    return 0;
}

/** recv property post response message from cloud **/
static int user_report_reply_event_handler(const int devid, const int msgid, const int code, const char *reply,
                                           const int reply_len)
{
    EXAMPLE_TRACE("Message Post Reply Received, Message ID: %d, Code: %d, Reply: %.*s", msgid, code,
                  reply_len,
                  (reply == NULL) ? ("NULL") : (reply));
    return 0;
}

/** recv event post response message from cloud **/
static int user_trigger_event_reply_event_handler(const int devid, const int msgid, const int code, const char *eventid,
                                                  const int eventid_len, const char *message, const int message_len)
{
    EXAMPLE_TRACE("Trigger Event Reply Received, Message ID: %d, Code: %d, EventID: %.*s, Message: %.*s",
                  msgid, code,
                  eventid_len,
                  eventid, message_len, message);

    return 0;
}

/** recv event post response message from cloud **/
static int user_property_set_event_handler(const int devid, const char *request, const int request_len)
{
    int res = 0;
    EXAMPLE_TRACE("Property Set Received, Request: %s", request);

    cJSON * root = RT_NULL;
    cJSON * ModeControl = RT_NULL;

    root = cJSON_Parse((unsigned char *)request);
    if(root == RT_NULL)
    {
        rt_kprintf("parse failed.\n");
        return -1;
    }
    ModeControl = cJSON_GetObjectItem(root, "ModeControl");
    rt_kprintf("ModeControl: %d\n",ModeControl->valueint);

    switch (ModeControl->valueint)
    {
    case 1:
        rt_kprintf("open\n");
        rt_pin_write(OPEN_PIN, PIN_HIGH);
        rt_thread_mdelay(500);
        rt_pin_write(OPEN_PIN,PIN_LOW);
        break;
    case 2:
        rt_kprintf("close\n");
        rt_pin_write(CLOSE_PIN, PIN_HIGH);
        rt_thread_mdelay(500);
        rt_pin_write(CLOSE_PIN,PIN_LOW);
        break;
    case 3:
        rt_kprintf("stop\n");
        rt_pin_write(STOP_PIN, PIN_HIGH);
        rt_thread_mdelay(500);
        rt_pin_write(STOP_PIN,PIN_LOW);
        break;
    case 4:
        rt_kprintf("lock\n");
        rt_pin_write(LOCK_PIN, PIN_HIGH);
        rt_thread_mdelay(500);
        rt_pin_write(LOCK_PIN,PIN_LOW);
        break;
    default:
        break;
    }

    // res = IOT_Linkkit_Report(EXAMPLE_MASTER_DEVID, ITM_MSG_POST_PROPERTY,
    //                          (unsigned char *)request, request_len);
    // EXAMPLE_TRACE("Post Property Message ID: %d", res);

    return 0;
}

static int user_service_request_event_handler(const int devid, const char *serviceid, const int serviceid_len,
                                              const char *request, const int request_len,
                                              char **response, int *response_len)
{
    int add_result = 0;
    cJSON *root = NULL, *item_number_a = NULL, *item_number_b = NULL;
    const char *response_fmt = "{\"Result\": %d}";

    EXAMPLE_TRACE("Service Request Received, Service ID: %.*s, Payload: %s", serviceid_len, serviceid, request);

    /* Parse Root */
    root = cJSON_Parse(request);
    if (root == NULL || !cJSON_IsObject(root))
    {
        EXAMPLE_TRACE("JSON Parse Error");
        return -1;
    }

    if (strlen("Operation_Service") == serviceid_len && memcmp("Operation_Service", serviceid, serviceid_len) == 0)
    {
        /* Parse NumberA */
        item_number_a = cJSON_GetObjectItem(root, "NumberA");
        if (item_number_a == NULL || !cJSON_IsNumber(item_number_a))
        {
            cJSON_Delete(root);
            return -1;
        }
        EXAMPLE_TRACE("NumberA = %d", item_number_a->valueint);

        /* Parse NumberB */
        item_number_b = cJSON_GetObjectItem(root, "NumberB");
        if (item_number_b == NULL || !cJSON_IsNumber(item_number_b))
        {
            cJSON_Delete(root);
            return -1;
        }
        EXAMPLE_TRACE("NumberB = %d", item_number_b->valueint);

        add_result = item_number_a->valueint + item_number_b->valueint;

        /* Send Service Response To Cloud */
        *response_len = strlen(response_fmt) + 10 + 1;
        *response = (char *)HAL_Malloc(*response_len);
        if (*response == NULL)
        {
            EXAMPLE_TRACE("Memory Not Enough");
            return -1;
        }
        memset(*response, 0, *response_len);
        HAL_Snprintf(*response, *response_len, response_fmt, add_result);
        *response_len = strlen(*response);
    }

    cJSON_Delete(root);
    return 0;
}

static int user_timestamp_reply_event_handler(const char *timestamp)
{
    EXAMPLE_TRACE("Current Timestamp: %s", timestamp);

    return 0;
}

/** fota event handler **/
static int user_fota_event_handler(int type, const char *version)
{
    char buffer[128] = {0};
    int buffer_length = 128;

    /* 0 - new firmware exist, query the new firmware */
    if (type == 0)
    {
        EXAMPLE_TRACE("New Firmware Version: %s", version);

        IOT_Linkkit_Query(EXAMPLE_MASTER_DEVID, ITM_MSG_QUERY_FOTA_DATA, (unsigned char *)buffer, buffer_length);
    }

    return 0;
}

/* cota event handler */
static int user_cota_event_handler(int type, const char *config_id, int config_size, const char *get_type,
                                   const char *sign, const char *sign_method, const char *url)
{
    char buffer[128] = {0};
    int buffer_length = 128;

    /* type = 0, new config exist, query the new config */
    if (type == 0)
    {
        EXAMPLE_TRACE("New Config ID: %s", config_id);
        EXAMPLE_TRACE("New Config Size: %d", config_size);
        EXAMPLE_TRACE("New Config Type: %s", get_type);
        EXAMPLE_TRACE("New Config Sign: %s", sign);
        EXAMPLE_TRACE("New Config Sign Method: %s", sign_method);
        EXAMPLE_TRACE("New Config URL: %s", url);

        IOT_Linkkit_Query(EXAMPLE_MASTER_DEVID, ITM_MSG_QUERY_COTA_DATA, (unsigned char *)buffer, buffer_length);
    }

    return 0;
}

void user_post_property(void)
{
    static int cnt = 0;
    int res = 0;

    char property_payload[30] = {0};
    HAL_Snprintf(property_payload, sizeof(property_payload), "{\"Counter\": %d}", cnt++);

    res = IOT_Linkkit_Report(EXAMPLE_MASTER_DEVID, ITM_MSG_POST_PROPERTY,
                             (unsigned char *)property_payload, strlen(property_payload));

    EXAMPLE_TRACE("Post Property Message ID: %d", res);
}

void user_post_event(void)
{
    int res = 0;
    char *event_id = "HardwareError";
    char *event_payload = "{\"ErrorCode\": 0}";

    res = IOT_Linkkit_TriggerEvent(EXAMPLE_MASTER_DEVID, event_id, strlen(event_id),
                                   event_payload, strlen(event_payload));
    EXAMPLE_TRACE("Post Event Message ID: %d", res);
}

void user_deviceinfo_update(void)
{
    int res = 0;
    char *device_info_update = "[{\"attrKey\":\"abc\",\"attrValue\":\"hello,world\"}]";

    res = IOT_Linkkit_Report(EXAMPLE_MASTER_DEVID, ITM_MSG_DEVICEINFO_UPDATE,
                             (unsigned char *)device_info_update, strlen(device_info_update));
    EXAMPLE_TRACE("Device Info Update Message ID: %d", res);
}

void user_deviceinfo_delete(void)
{
    int res = 0;
    char *device_info_delete = "[{\"attrKey\":\"abc\"}]";

    res = IOT_Linkkit_Report(EXAMPLE_MASTER_DEVID, ITM_MSG_DEVICEINFO_DELETE,
                             (unsigned char *)device_info_delete, strlen(device_info_delete));
    EXAMPLE_TRACE("Device Info Delete Message ID: %d", res);
}

#include <arpa/inet.h> /* 包含 ip_addr_t 等地址相关的头文件 */
#include <netdev.h>    /* 包含全部的 netdev 相关操作接口函数 */

int wait_for_internet()
{
    struct netdev *netdev = RT_NULL;

    while (1)
    {
        netdev = netdev_get_by_name("esp0");
        if (netdev != RT_NULL)
        {
            rt_kprintf("netdev is found!\r\n");

            break;
        }
        else
        {
            rt_kprintf("netdev is not found!\r\n");
            rt_thread_mdelay(500);
        }
    }

    while (1)
    {
        rt_bool_t res = netdev_is_internet_up(netdev);
        if (res == RT_TRUE)
        {
            rt_kprintf("internet is ready!\r\n");
            break;
        }
        rt_thread_mdelay(500);
    }

    rt_thread_mdelay(5000);
}


#define WDT_DEVICE_NAME    "wdt"    /* 看门狗设备名称 */
static rt_device_t wdg_dev;         /* 看门狗设备句柄 */

int wdt_init()
{
    rt_err_t ret = RT_EOK;
    rt_uint32_t timeout = 10;

    wdg_dev = rt_device_find(WDT_DEVICE_NAME);
    if(!wdg_dev)
    {
        rt_kprintf("find %s failed!\n", WDT_DEVICE_NAME);
        return RT_ERROR;
    }

    /* 设置看门狗溢出时间 */
    ret = rt_device_control(wdg_dev, RT_DEVICE_CTRL_WDT_SET_TIMEOUT, &timeout);
    if (ret != RT_EOK)
    {
        rt_kprintf("set %s timeout failed!\n", WDT_DEVICE_NAME);
        return RT_ERROR;
    }

    /* 启动看门狗 */
    ret = rt_device_control(wdg_dev, RT_DEVICE_CTRL_WDT_START, RT_NULL);
    if (ret != RT_EOK)
    {
        rt_kprintf("start %s failed!\n", WDT_DEVICE_NAME);
        return -RT_ERROR;
    }

    return ret;
}





void linkkit_solo_main(void* parameter)
{

    rt_pin_mode(OPEN_PIN,PIN_MODE_OUTPUT);
    rt_pin_mode(CLOSE_PIN,PIN_MODE_OUTPUT);
    rt_pin_mode(STOP_PIN,PIN_MODE_OUTPUT);
    rt_pin_mode(LOCK_PIN,PIN_MODE_OUTPUT);

    rt_pin_write(OPEN_PIN,PIN_LOW);
    rt_pin_write(CLOSE_PIN,PIN_LOW);
    rt_pin_write(STOP_PIN,PIN_LOW);
    rt_pin_write(LOCK_PIN,PIN_LOW);


    wait_for_internet();
    wdt_init();

    int res = 0;
    int cnt = 0;
    iotx_linkkit_dev_meta_info_t master_meta_info;
    int domain_type = 0, dynamic_register = 0, post_reply_need = 0;

#ifdef ATM_ENABLED
    if (IOT_ATM_Init() < 0)
    {
        EXAMPLE_TRACE("IOT ATM init failed!\n");
        return -1;
    }
#endif

    memset(&g_user_example_ctx, 0, sizeof(user_example_ctx_t));

    HAL_GetProductKey(PRODUCT_KEY);
    HAL_GetProductSecret(PRODUCT_SECRET);
    HAL_GetDeviceName(DEVICE_NAME);
    HAL_GetDeviceSecret(DEVICE_SECRET);
    memset(&master_meta_info, 0, sizeof(iotx_linkkit_dev_meta_info_t));
    memcpy(master_meta_info.product_key, PRODUCT_KEY, strlen(PRODUCT_KEY));
    memcpy(master_meta_info.product_secret, PRODUCT_SECRET, strlen(PRODUCT_SECRET));
    memcpy(master_meta_info.device_name, DEVICE_NAME, strlen(DEVICE_NAME));
    memcpy(master_meta_info.device_secret, DEVICE_SECRET, strlen(DEVICE_SECRET));

    IOT_SetLogLevel(IOT_LOG_INFO);

    /* Register Callback */
    IOT_RegisterCallback(ITE_CONNECT_SUCC, user_connected_event_handler);
    IOT_RegisterCallback(ITE_DISCONNECTED, user_disconnected_event_handler);
    IOT_RegisterCallback(ITE_SERVICE_REQUEST, user_service_request_event_handler);
    IOT_RegisterCallback(ITE_PROPERTY_SET, user_property_set_event_handler);
    // IOT_RegisterCallback(ITE_REPORT_REPLY, user_report_reply_event_handler);
    IOT_RegisterCallback(ITE_TRIGGER_EVENT_REPLY, user_trigger_event_reply_event_handler);
    IOT_RegisterCallback(ITE_TIMESTAMP_REPLY, user_timestamp_reply_event_handler);
    IOT_RegisterCallback(ITE_INITIALIZE_COMPLETED, user_initialized);
    IOT_RegisterCallback(ITE_FOTA, user_fota_event_handler);
    IOT_RegisterCallback(ITE_COTA, user_cota_event_handler);

    domain_type = IOTX_CLOUD_REGION_SHANGHAI;
    IOT_Ioctl(IOTX_IOCTL_SET_DOMAIN, (void *)&domain_type);

    /* Choose Login Method */
    dynamic_register = 0;
    IOT_Ioctl(IOTX_IOCTL_SET_DYNAMIC_REGISTER, (void *)&dynamic_register);

    /* post reply doesn't need */
    post_reply_need = 1;
    IOT_Ioctl(IOTX_IOCTL_RECV_EVENT_REPLY, (void *)&post_reply_need);

    /* Create Master Device Resources */
    g_user_example_ctx.master_devid = IOT_Linkkit_Open(IOTX_LINKKIT_DEV_TYPE_MASTER, &master_meta_info);
    if (g_user_example_ctx.master_devid < 0)
    {
        EXAMPLE_TRACE("IOT_Linkkit_Open Failed\n");
        return ;
    }

    rt_kprintf("== 1 ==\n");

    /* Start Connect Aliyun Server */
    res = IOT_Linkkit_Connect(g_user_example_ctx.master_devid);
    if (res < 0)
    {
        EXAMPLE_TRACE("IOT_Linkkit_Connect Failed\n");
        return ;
    }
    rt_kprintf("== 2 ==\n");

    while (1)
    { 
        IOT_Linkkit_Yield(EXAMPLE_YIELD_TIMEOUT_MS);

        // /* Post Proprety Example */
        // if ((cnt % 2) == 0) {
        //     user_post_property();
        // }

        // /* Post Event Example */
        // if ((cnt % 10) == 0) {
        //     user_post_event();
        // }

        // if (++cnt > 3600) {
        //     break;
        // }
        /* 在空闲线程的回调函数里喂狗 */
        rt_device_control(wdg_dev, RT_DEVICE_CTRL_WDT_KEEPALIVE, NULL);
        //rt_kprintf("feed dog\n");

        rt_thread_mdelay(100);
    }

}
MSH_CMD_EXPORT_ALIAS(linkkit_solo_main, ali_linkkit_solo_sample, ali linkkit sole sample);

int aliyun_start()
{
    // linkkit_solo_main();
    rt_thread_t tid = rt_thread_create("aliyun",linkkit_solo_main,RT_NULL,4096,20,20);
    rt_thread_startup(tid);
}
MSH_CMD_EXPORT(aliyun_start, aliyun start);
INIT_APP_EXPORT(aliyun_start);

#endif
