#include <stdlib.h>
#include <string.h>

#include "simble.h"
#include "onboard-led.h"


struct ble_gap_advdata {
        uint8_t length;
        uint8_t data[BLE_GAP_ADV_MAX_SIZE];
};

struct ble_gap_ad_header {
        uint8_t payload_length;
        uint8_t type;
} __packed;

struct ble_gap_ad_flags {
        struct ble_gap_ad_header;
        uint8_t flags;
} __packed;

struct ble_gap_ad_name {
        struct ble_gap_ad_header;
        uint8_t name[BLE_GAP_DEVNAME_MAX_LEN];
} __packed;



static struct service_desc *services;
static uint16_t current_conn_handle = BLE_CONN_HANDLE_INVALID;
static ble_dfu_t m_dfus; /**< Structure used to identify the DFU service. */
static dm_application_instance_t         m_app_handle;                              /**< Application identifier allocated by device manager */

static uint32_t
simble_add_advdata(const struct ble_gap_ad_header *data, struct ble_gap_advdata *advdata)
{
        size_t elem_length = data->payload_length + sizeof(*data);
        size_t final_length = advdata->length + elem_length;

        if (final_length > sizeof(advdata->data))
                return NRF_ERROR_DATA_SIZE;

        memcpy(&advdata->data[advdata->length], data, elem_length);
        advdata->data[advdata->length] = elem_length - 1;

        advdata->length += elem_length;

        return NRF_SUCCESS;
}

void
simble_adv_start(void)
{
        struct ble_gap_advdata advdata = { .length = 0 };

        struct ble_gap_ad_flags flags = {
                .payload_length = 1,
                .type = BLE_GAP_AD_TYPE_FLAGS,
                .flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
        };
        simble_add_advdata(&flags, &advdata);
        struct ble_gap_ad_name name;
        uint16_t namelen = sizeof(advdata);
        if (sd_ble_gap_device_name_get(name.name, &namelen) != NRF_SUCCESS)
                namelen = 0;
        name.type =  BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME;
        name.payload_length = namelen;
	simble_add_advdata(&name, &advdata);

        sd_ble_gap_adv_data_set(advdata.data, advdata.length, NULL, 0);
        ble_gap_adv_params_t adv_params = {
                .type = BLE_GAP_ADV_TYPE_ADV_IND,
                .fp = BLE_GAP_ADV_FP_ANY,
                .interval = 0x400,
        };
        sd_ble_gap_adv_start(&adv_params);
        onboard_led(ONBOARD_LED_ON);
}

static void
simble_srv_tx_init(void)
{
        uint16_t srv_handle;
        ble_uuid_t srv_uuid = {.type = BLE_UUID_TYPE_BLE, .uuid = 0x1804};
        sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                 &srv_uuid,
                                 &srv_handle);

        ble_gatts_char_handles_t chr_handles;
        ble_gatts_char_md_t char_meta = {.char_props = {.read = 1}};
        ble_uuid_t chr_uuid = {.type = BLE_UUID_TYPE_BLE, .uuid = 0x2a07};
        ble_gatts_attr_md_t chr_attr_meta = {
                .vloc = BLE_GATTS_VLOC_STACK,
        };
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&chr_attr_meta.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&chr_attr_meta.write_perm);
        uint8_t tx_val = 0;
        ble_gatts_attr_t chr_attr = {
                .p_uuid = &chr_uuid,
                .p_attr_md = &chr_attr_meta,
                .init_offs = 0,
                .init_len = sizeof(tx_val),
                .max_len = sizeof(tx_val),
                .p_value = &tx_val,
        };
        sd_ble_gatts_characteristic_add(srv_handle,
                                        &char_meta,
                                        &chr_attr,
                                        &chr_handles);
}


/**@brief Function for loading application-specific context after establishing a secure connection.
 *
 * @details This function will load the application context and check if the ATT table is marked as 
 *          changed. If the ATT table is marked as changed, a Service Changed Indication
 *          is sent to the peer if the Service Changed CCCD is set to indicate.
 *
 * @param[in] p_handle The Device Manager handle that identifies the connection for which the context 
 *                     should be loaded.
 */
static void app_context_load(dm_handle_t const * p_handle)
{
    uint32_t                 err_code;
    static uint32_t          context_data;
    dm_application_context_t context;

    context.len    = sizeof(context_data);
    context.p_data = (uint8_t *)&context_data;

    err_code = dm_application_context_get(p_handle, &context);
    if (err_code == NRF_SUCCESS)
    {
        // Send Service Changed Indication if ATT table has changed.
        if ((context_data & (DFU_APP_ATT_TABLE_CHANGED << DFU_APP_ATT_TABLE_POS)) != 0)
        {
            err_code = sd_ble_gatts_service_changed(current_conn_handle, APP_SERVICE_HANDLE_START, BLE_HANDLE_MAX);
            if ((err_code != NRF_SUCCESS) &&
                (err_code != BLE_ERROR_INVALID_CONN_HANDLE) &&
                (err_code != NRF_ERROR_INVALID_STATE) &&
                (err_code != BLE_ERROR_NO_TX_BUFFERS) &&
                (err_code != NRF_ERROR_BUSY) &&
                (err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING))
            {
                segger_rtt_printf("sd_ble_gatts_service_changed err: %d \n",err_code);
            }
        }

        err_code = dm_application_context_delete(p_handle);
        segger_rtt_printf("dm_application_context_delete err: %d \n",err_code);
    }
    else if (err_code == DM_NO_APP_CONTEXT)
    {
        // No context available. Ignore.
    }
    else
    {
        segger_rtt_printf("dm_application_context_get err: %d \n",err_code);
    }
}



/**@brief Function for handling the Device Manager events.
 *
 * @param[in] p_evt  Data associated to the device manager event.
 */
static uint32_t device_manager_evt_handler(dm_handle_t const * p_handle,
                                           dm_event_t const  * p_event,
                                           ret_code_t        event_result)
{
    segger_rtt_printf("device_manager_evt_handler err: %d \n",event_result);


    if (p_event->event_id == DM_EVT_LINK_SECURED)
    {
        app_context_load(p_handle);
    }
// BLE_DFU_APP_SUPPORT

    return NRF_SUCCESS;
}

/**@brief Function for the Device Manager initialization.
 */
static void simble_device_manager_init(void)
{
    uint32_t               err_code;
    dm_init_param_t        init_data;
    dm_application_param_t register_param;

    // Initialize persistent storage module.
    err_code = pstorage_init();
    segger_rtt_printf("pstorage_init err: %d \n",err_code);

    err_code = dm_init(&init_data);
    segger_rtt_printf("dm_init err: %d \n",err_code);

    memset(&register_param.sec_param, 0, sizeof(ble_gap_sec_params_t));

    register_param.sec_param.bond         = SEC_PARAM_BOND;
    register_param.sec_param.mitm         = SEC_PARAM_MITM;
    register_param.sec_param.io_caps      = SEC_PARAM_IO_CAPABILITIES;
    register_param.sec_param.oob          = SEC_PARAM_OOB;
    register_param.sec_param.min_key_size = SEC_PARAM_MIN_KEY_SIZE;
    register_param.sec_param.max_key_size = SEC_PARAM_MAX_KEY_SIZE;
    register_param.evt_handler            = device_manager_evt_handler;
    register_param.service_type           = DM_PROTOCOL_CNTXT_GATT_SRVR_ID;

    err_code = dm_register(&m_app_handle, &register_param);
    segger_rtt_printf("dm_register err: %d \n",err_code);
}


/**@brief Function for stopping advertising.
 */
static void advertising_stop(void)
{
    uint32_t err_code;

    err_code = sd_ble_gap_adv_stop();
    segger_rtt_printf("advertising_stop err: %d \n",err_code);

}



/** @snippet [DFU BLE Reset prepare] */
/**@brief Function for preparing for system reset.
 *
 * @details This function implements @ref dfu_app_reset_prepare_t. It will be called by 
 *          @ref dfu_app_handler.c before entering the bootloader/DFU.
 *          This allows the current running application to shut down gracefully.
 */
static void reset_prepare(void)
{
    uint32_t err_code;

    if (current_conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        // Disconnect from peer.
        err_code = sd_ble_gap_disconnect(current_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        segger_rtt_printf("sd_ble_gap_disconnect err: %d \n",err_code);

    }
    else
    {
        // If not connected, the device will be advertising. Hence stop the advertising.
        advertising_stop();
    }

    /*err_code = ble_conn_params_stop();
    segger_rtt_printf("sd_ble_gap_disconnect err: %d \n",err_code);*/ // Didn't use conn_params

    nrf_delay_ms(500);
}
/** @snippet [DFU BLE Reset prepare] */


static void simble_dfu_init(void)
{
    uint32_t err_code;
    /** @snippet [DFU BLE Service initialization] */
    ble_dfu_init_t dfus_init;

    // Initialize the Device Firmware Update Service.
    memset(&dfus_init, 0, sizeof(dfus_init));

    dfus_init.evt_handler   = dfu_app_on_dfu_evt;
    dfus_init.error_handler = NULL;
    dfus_init.evt_handler   = dfu_app_on_dfu_evt;
    dfus_init.revision      = DFU_REVISION;

    err_code = ble_dfu_init(&m_dfus, &dfus_init);
    segger_rtt_printf("ble_dfu_init err: %d \n",err_code);

    dfu_app_reset_prepare_set(reset_prepare);
    dfu_app_dm_appl_instance_set(m_app_handle);
    /** @snippet [DFU BLE Service initialization] */
}

uint8_t
simble_get_vendor_uuid_class(void)
{
        ble_uuid128_t vendorid = {{0x13,0xb0,0xa0,0x71,0xfe,0x62,0x4c,0x01,0xaa,0x4d,0xd8,0x03,0,0,0x0b,0xd0}};
        static uint8_t vendor_type = BLE_UUID_TYPE_UNKNOWN;

        if (vendor_type != BLE_UUID_TYPE_UNKNOWN)
                return (vendor_type);

        sd_ble_uuid_vs_add(&vendorid, &vendor_type);
        return (vendor_type);
}


void
simble_srv_register(struct service_desc *s)
{
        s->next = services;
        services = s;

        sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                 &s->uuid,
                                 &s->handle);

        for (int i = 0; i < s->char_count; ++i) {
                struct char_desc *c = &s->chars[i];

                int have_write = c->write_cb != NULL;
                ble_gatts_char_md_t char_meta = {
                        .char_props = {
                                .read = 1,
                                /* XXX customizable */
                                .write = have_write,
                                .write_wo_resp = have_write,
                                .auth_signed_wr = have_write,
                        },
                        .p_char_user_desc = (uint8_t *)c->desc,
                        .char_user_desc_size = strlen(c->desc),
                        .char_user_desc_max_size = strlen(c->desc),
                        .p_char_pf = c->format.format != 0 ? &c->format : NULL,
                };
                ble_gatts_attr_md_t chr_attr_meta = {
                        .vloc = BLE_GATTS_VLOC_STACK,
                        .rd_auth = 1,
                        .wr_auth = 1,
                };
                BLE_GAP_CONN_SEC_MODE_SET_OPEN(&chr_attr_meta.read_perm);
                BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&chr_attr_meta.write_perm);
                if (have_write)
                        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&chr_attr_meta.write_perm);

                ble_gatts_attr_t chr_attr = {
                        .p_uuid = &c->uuid,
                        .p_attr_md = &chr_attr_meta,
                        .init_offs = 0,
                        .init_len = 0,
                        .max_len = c->length,
                };
                ble_gatts_char_handles_t chr_handles;
                sd_ble_gatts_characteristic_add(s->handle,
                                                &char_meta,
                                                &chr_attr,
                                                &chr_handles);
                c->handle = chr_handles.value_handle;
        }
}

void
simble_srv_init(struct service_desc *s, uint8_t type, uint16_t id)
{
        *s = (struct service_desc){
                .uuid = {.type = type,
                         .uuid = id},
                .char_count = 0,
        };
};

void
simble_srv_char_add(struct service_desc *s, struct char_desc *c, uint8_t type, uint16_t id, const char *desc, uint16_t length)
{
        *c = (struct char_desc){
                .uuid = {
                        .type = type,
                        .uuid = id
                },
                .desc = desc,
                .length = length,
        };
        s->char_count++;
}

void
simble_srv_char_attach_format(struct char_desc *c, uint8_t format, int8_t exponent, uint16_t unit)
{
        c->format = (ble_gatts_char_pf_t){
                .format = format,
                .exponent = exponent,
                .unit = unit,
        };
}

void
simble_srv_char_update(struct char_desc *c, void *val)
{
        ble_gatts_value_t vt = {
                .len = c->length,
                .offset = 0,
                .p_value = val
        };
        sd_ble_gatts_value_set(current_conn_handle, c->handle, &vt);
}

static struct service_desc *
srv_find_by_uuid(ble_uuid_t *uuid)
{
        struct service_desc *s = services;

        for (; s != NULL; s = s->next) {
                if (memcmp(&s->uuid, uuid, sizeof(uuid)) == 0)
                        break;
        }

        return (s);
}

static struct char_desc *
srv_find_char_by_uuid(struct service_desc *s, ble_uuid_t *uuid)
{
        struct char_desc *c = s->chars;

        for (int i = 0; i < s->char_count; ++i, ++c) {
                if (memcmp(&c->uuid, uuid, sizeof(uuid)) == 0)
                        return (c);
        }
        return (NULL);
}

typedef void (srv_foreach_cb_t)(struct service_desc *s);

static void
srv_foreach_srv(srv_foreach_cb_t *cb)
{
        struct service_desc *s = services;

        for (; s != NULL; s = s->next)
                cb(s);
}

static void
srv_notify_connect(struct service_desc *s)
{
        if (s->connect_cb)
                s->connect_cb(s);
}

static void
srv_notify_disconnect(struct service_desc *s)
{
        if (s->disconnect_cb)
                s->disconnect_cb(s);
}

static void
simble_app_disconnected(void)
{
        simble_adv_start();
}

static void
srv_on_event(ble_evt_t *evt)
{
	struct service_desc *s;
	struct char_desc *c;

	switch (evt->header.evt_id) {
        case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST: {
                ble_gatts_rw_authorize_reply_params_t auth_reply = {
                        .type = evt->evt.gatts_evt.params.authorize_request.type,
                };
                if (auth_reply.type == BLE_GATTS_AUTHORIZE_TYPE_READ) {
                        ble_gatts_attr_context_t *ctx = &evt->evt.gatts_evt.params.authorize_request.request.read.context;
                        s = srv_find_by_uuid(&ctx->srvc_uuid);
                        c = srv_find_char_by_uuid(s, &ctx->char_uuid);
                        auth_reply.params.read.gatt_status = BLE_GATT_STATUS_SUCCESS;
                        if (c->read_cb) {
                                auth_reply.params.read.update = 1;
                                c->read_cb(s, c, (void*)&auth_reply.params.read.p_data, &auth_reply.params.read.len);
                        }
                } else {
                        auth_reply.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;
                }
                sd_ble_gatts_rw_authorize_reply(evt->evt.gatts_evt.conn_handle, &auth_reply);
                break;
        }
        case BLE_GAP_EVT_CONNECTED:
                current_conn_handle = evt->evt.gap_evt.conn_handle;
                srv_foreach_srv(srv_notify_connect);
		onboard_led(ONBOARD_LED_OFF);
                break;
        case BLE_GAP_EVT_DISCONNECTED:
                current_conn_handle = BLE_CONN_HANDLE_INVALID;
                srv_foreach_srv(srv_notify_disconnect);
		simble_app_disconnected();
                break;
        /*case BLE_GATTS_EVT_WRITE:
                s = srv_find_by_uuid(&evt->evt.gatts_evt.params.write.context.srvc_uuid);
                c = srv_find_char_by_uuid(s, &evt->evt.gatts_evt.params.write.context.char_uuid);
                c->write_cb(s, c, evt->evt.gatts_evt.params.write.data, evt->evt.gatts_evt.params.write.len);
                break;*/
        }
}

static void
srv_handle_ble_event(ble_evt_t *evt)
{


	/** @snippet [Propagating BLE Stack events to DFU Service] */
	ble_dfu_on_ble_evt(&m_dfus, evt);
	/** @snippet [Propagating BLE Stack events to DFU Service] */
	dm_ble_evt_handler(evt);
	srv_on_event(evt);
	
       /* */
}




void
simble_init(const char *name)
{
	uint32_t err_code;
        //sd_softdevice_enable(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM, NULL); /* XXX assertion handler */
	SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM, NULL);
        ble_enable_params_t ble_params = {
#if defined(SD120)
                .gap_enable_params = {
                        .role = BLE_GAP_ROLE_PERIPH,
                },
#endif
                .gatts_enable_params = {
                        .service_changed = 1,
                },
        };
        sd_ble_enable(&ble_params);

        ble_gap_conn_sec_mode_t mode;
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&mode);
        sd_ble_gap_device_name_set(&mode, (const uint8_t *)name, strlen(name));

	err_code = softdevice_ble_evt_handler_set(srv_handle_ble_event);
	segger_rtt_printf("softdevice_ble_evt_handler_set err: %d \n",err_code);

        simble_srv_tx_init();
	simble_dfu_init();
	simble_device_manager_init();
}


void
simble_process_event_loop(void)
{
        for (;;) {


		NRF_GPIO->PIN_CNF[2] = GPIO_PIN_CNF_DIR_Output | GPIO_PIN_CNF_INPUT_Disconnect;
        
                NRF_GPIO->OUTSET = (1 << 2);

                
		sd_app_evt_wait();
		NRF_GPIO->OUTCLR = (1 << 2);
               
        }
}