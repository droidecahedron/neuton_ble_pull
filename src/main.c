/*
 * neuton_ble_pull
 * main.c
 * ble application, mocks some sensor data and sends it to le char.
 * includes a rx characteristic in some dev cfg is also needed.
 * uses dle, phy, conn param for bigger packets.
 * play w/ the connection params (or remove and use the commented config in prj.conf)
 * to get a connection situation that is best for your intended central.
 * auth: ddhd
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <dk_buttons_and_leds.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/printk.h>

#include <hal/nrf_gpio.h>
#include <zephyr/drivers/gpio.h>

#define GPIO1_NODE DT_NODELABEL(gpio1)
#define CPS_PIN 11
#define CHL_PIN 12
const struct device *gpio1_dev = DEVICE_DT_GET(GPIO1_NODE);

LOG_MODULE_REGISTER(neuton_pull_ble, LOG_LEVEL_INF);
#define DK_STATUS_LED DK_LED1
#define BLE_STATE_LED DK_LED2

#define SAMPLE_WINDOW 100 // 100 samples per chunk
#define SENS_SAMPLE_INTERVAL 10

// can also use zbus, or pass with work container w/ kernel work item.
struct sens_sample_msg
{
    uint8_t ip_dat[SAMPLE_WINDOW];
};
// 8 messages, 4-byte alignment
K_MSGQ_DEFINE(sens_msgq, sizeof(struct sens_sample_msg), 8, 4);

#define BLE_NOTIFY_INTERVAL K_MSEC(SENS_SAMPLE_INTERVAL *SAMPLE_WINDOW)
#define BLE_THREAD_STACK_SIZE 1024
#define BLE_THREAD_PRIORITY 5

// Declaration of custom GATT service and characteristics UUIDs
#define NEUTON_PULL_SERVICE_UUID BT_UUID_128_ENCODE(0x69e5204b, 0x8445, 0x5fca, 0xb332, 0xc13064b9dea2)
#define SENS_DAT_TX_CHARACTERISTIC_UUID BT_UUID_128_ENCODE(0x5E85012D, 0x7ea8, 0x4008, 0xb432, 0xb46096c049ba)
#define DEVC_CFG_RX_CHARACTERISTIC_UUID BT_UUID_128_ENCODE(0xDE70CF61, 0xd8ee, 0x4faf, 0x956b, 0xafb01c17d0be)

#define BT_UUID_NEUTON_PULL BT_UUID_DECLARE_128(NEUTON_PULL_SERVICE_UUID)
#define BT_UUID_SENS_DAT_TX BT_UUID_DECLARE_128(SENS_DAT_TX_CHARACTERISTIC_UUID)
#define BT_UUID_DEVC_CFG_RX BT_UUID_DECLARE_128(DEVC_CFG_RX_CHARACTERISTIC_UUID)

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME // from prj.conf
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

static const struct bt_le_adv_param *adv_param =
    BT_LE_ADV_PARAM((BT_LE_ADV_OPT_CONN | BT_LE_ADV_OPT_USE_IDENTITY), /* Connectable advertising and use
                                                                          identity address */
                    800,   /* Min Advertising Interval 500ms (800*0.625ms) 16383 max*/
                    801,   /* Max Advertising Interval 500.625ms (801*0.625ms) 16384 max*/
                    NULL); /* Set to NULL for undirected advertising */

static struct k_work adv_work;
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, NEUTON_PULL_SERVICE_UUID),
};

void on_le_param_updated(struct bt_conn *conn, uint16_t interval, uint16_t latency, uint16_t timeout)
{
    double connection_interval = interval * 1.25; // in ms
    uint16_t supervision_timeout = timeout * 10;  // in ms
    LOG_INF("Connection parameters updated: interval %.2f ms, latency %d intervals, timeout %d ms", connection_interval,
            latency, supervision_timeout);
}

void on_le_phy_updated(struct bt_conn *conn, struct bt_conn_le_phy_info *param)
{
    // PHY Updated
    if (param->tx_phy == BT_CONN_LE_TX_POWER_PHY_1M)
    {
        LOG_INF("PHY updated. New PHY: 1M");
    }
    else if (param->tx_phy == BT_CONN_LE_TX_POWER_PHY_2M)
    {
        LOG_INF("PHY updated. New PHY: 2M");
    }

    // shouldnt happen
    else if (param->tx_phy == BT_CONN_LE_TX_POWER_PHY_CODED_S8)
    {
        LOG_INF("PHY updated. New PHY: Long Range");
    }
}

void on_le_data_len_updated(struct bt_conn *conn, struct bt_conn_le_data_len_info *info)
{
    uint16_t tx_len = info->tx_max_len;
    uint16_t tx_time = info->tx_max_time;
    uint16_t rx_len = info->rx_max_len;
    uint16_t rx_time = info->rx_max_time;
    LOG_INF("Data length updated. Length %d/%d bytes, time %d/%d us", tx_len, rx_len, tx_time, rx_time);
}

/*This function is called whenever the Client Characteristic Control Descriptor
(CCCD) has been changed by the GATT client, for each of the characteristics*/
static void on_cccd_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    ARG_UNUSED(attr);
    switch (value)
    {
    case BT_GATT_CCC_NOTIFY:
        break;
    case 0:
        break;
    default:
        LOG_ERR("Error, CCCD has been set to an invalid value");
    }
}

// fn called when dev cfg wr characteristic has been written to by a client
static ssize_t on_recv_cfg(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len,
                           uint16_t offset, uint8_t flags)
{
    const uint8_t *buffer = buf;

    printk("Received cfg wr data, handle %d, conn %p, len %d, data: 0x", attr->handle, conn, len);
    for (uint8_t i = 0; i < len; i++)
    {
        printk("%02X", buffer[i]);
    }
    printk("\n");

    return len;
}

BT_GATT_SERVICE_DEFINE(neuton_pull, BT_GATT_PRIMARY_SERVICE(BT_UUID_NEUTON_PULL),
                       BT_GATT_CHARACTERISTIC(BT_UUID_SENS_DAT_TX, BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ, NULL, NULL,
                                              NULL),
                       BT_GATT_CCC(on_cccd_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
                       BT_GATT_CHARACTERISTIC(BT_UUID_DEVC_CFG_RX, BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                                              BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, NULL, on_recv_cfg, NULL), );

// BT globals and callbacks
struct bt_conn *m_connection_handle = NULL;
static struct bt_gatt_exchange_params exchange_params;
static void adv_work_handler(struct k_work *work)
{
    int err = bt_le_adv_start(adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err)
    {
        LOG_INF("Advertising failed to start (err %d)", err);
        return;
    }

    LOG_INF("Advertising successfully started");
}

static void advertising_start(void)
{
    k_work_submit(&adv_work);
}

static void recycled_cb(void)
{
    LOG_INF("Connection object available from previous conn. Disconnect is "
            "complete!");
    advertising_start();
}

static void update_phy(struct bt_conn *conn)
{
    int err;
    const struct bt_conn_le_phy_param preferred_phy = {
        .options = BT_CONN_LE_PHY_OPT_NONE,
        .pref_rx_phy = BT_GAP_LE_PHY_2M,
        .pref_tx_phy = BT_GAP_LE_PHY_2M,
    };
    err = bt_conn_le_phy_update(conn, &preferred_phy);
    if (err)
    {
        LOG_ERR("bt_conn_le_phy_update() returned %d", err);
    }
}

static void update_data_length(struct bt_conn *conn)
{
    int err;
    struct bt_conn_le_data_len_param my_data_len = {
        .tx_max_len = CONFIG_BT_CTLR_DATA_LENGTH_MAX,
        .tx_max_time = BT_GAP_DATA_TIME_MAX,
    };
    err = bt_conn_le_data_len_update(m_connection_handle, &my_data_len);
    if (err)
    {
        LOG_ERR("data_len_update failed (err %d)", err);
    }
}

static void exchange_func(struct bt_conn *conn, uint8_t att_err, struct bt_gatt_exchange_params *params)
{
    LOG_INF("MTU exchange %s", att_err == 0 ? "successful" : "failed");
    if (!att_err)
    {
        uint16_t payload_mtu = bt_gatt_get_mtu(conn) - 3; // 3 bytes used for Attribute headers.
        LOG_INF("New MTU: %d bytes", payload_mtu);
    }
}

static void update_mtu(struct bt_conn *conn)
{
    int err;
    exchange_params.func = exchange_func;

    err = bt_gatt_exchange_mtu(conn, &exchange_params);
    if (err)
    {
        LOG_ERR("bt_gatt_exchange_mtu failed (err %d)", err);
    }
}

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err)
    {
        LOG_WRN("Connection failed (err %u)", err);
        return;
    }
    m_connection_handle = bt_conn_ref(conn);
    LOG_INF("Connected");

    struct bt_conn_info info;
    err = bt_conn_get_info(m_connection_handle, &info);
    if (err)
    {
        LOG_ERR("bt_conn_get_info() returned %d", err);
        return;
    }
    double connection_interval = info.le.interval * 1.25; // in ms
    uint16_t supervision_timeout = info.le.timeout * 10;  // in ms
    LOG_INF("Connection parameters: interval %.2f ms, latency %d intervals, timeout %d ms", connection_interval,
            info.le.latency, supervision_timeout);

    update_phy(m_connection_handle);
    k_sleep(K_MSEC(1000)); // Delay added to avoid link layer collisions.
    update_data_length(m_connection_handle);
    update_mtu(m_connection_handle);

    dk_set_led_on(BLE_STATE_LED);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Disconnected (reason %u)", reason);
    bt_conn_unref(m_connection_handle);
    m_connection_handle = NULL;
    dk_set_led_off(BLE_STATE_LED);
}

struct bt_conn_cb connection_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
    .recycled = recycled_cb,
    .le_param_updated = on_le_param_updated,
    .le_phy_updated = on_le_phy_updated,
    .le_data_len_updated = on_le_data_len_updated,
};

static void ble_sens_report(struct bt_conn *conn, const uint8_t *data, uint16_t len)
{
    const struct bt_gatt_attr *attr = &neuton_pull.attrs[2];
    struct bt_gatt_notify_params params = {
        .uuid = BT_UUID_SENS_DAT_TX, .attr = attr, .data = data, .len = len, .func = NULL};

    if (bt_gatt_is_subscribed(conn, attr, BT_GATT_CCC_NOTIFY))
    {
        if (bt_gatt_notify_cb(conn, &params))
        {
            LOG_ERR("Error, unable to send notification");
        }
    }
    else
    {
        LOG_WRN("Warning, notification not enabled for sens dat characteristic");
    }
}

void ble_write_thread(void)
{
    struct sens_sample_msg msg;
    for (;;)
    {
        // wait until ip dat rdy
        k_msgq_get(&sens_msgq, &msg, K_FOREVER);
        // printk("BLE thread received data: 0x");
        // for (uint8_t i = 0; i < SAMPLE_WINDOW; i++)
        // {
        //     printk(" %02X", msg.ip_dat[i]);
        // }
        // printk("\n");

        if (m_connection_handle) // if ble connection present
        {
            ble_sens_report(m_connection_handle, &msg.ip_dat[0], SAMPLE_WINDOW);
        }
        else
        {
            LOG_INF("BLE Thread does not detect an active BLE connection");
        }

        k_sleep(K_MSEC(5000));
    }
}

//~~~~ SENS ~~~~//
#define SENS_THREAD_STACK_SIZE 1024
#define SENS_THREAD_PRIORITY 5
#define SENS_SAMPLE_INTERVAL_MS K_MSEC(SENS_SAMPLE_INTERVAL)

void sens_sample_thread(void)
{
    int err;
    uint8_t iter = 0;

    struct sens_sample_msg msg;

    for (;;)
    {
        msg.ip_dat[0] = iter++;
        for (uint8_t i = 1; i < SAMPLE_WINDOW; i++)
        {
            msg.ip_dat[i] = i;
            k_sleep(SENS_SAMPLE_INTERVAL_MS);
        }

        //LOG_INF("sens thread sent %d samples", SAMPLE_WINDOW);
        err = k_msgq_put(&sens_msgq, &msg, K_FOREVER);
        if (err != 0)
        {
            LOG_ERR("issue putting message into kmsgq in sens thread");
        }
        k_sleep(K_MSEC(10));
    }
}

int main(void)
{
    int err;
    int blink = 0;

    err = dk_leds_init();
    if (err)
    {
        LOG_ERR("LEDs init failed (err %d)", err);
        return -1;
    }

    // Setting up Bluetooth
    err = bt_enable(NULL);
    if (err)
    {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return -1;
    }
    LOG_INF("Bluetooth initialized");
    if (IS_ENABLED(CONFIG_SETTINGS))
    {
        settings_load();
    }
    bt_conn_cb_register(&connection_callbacks);
    k_work_init(&adv_work, adv_work_handler);
    advertising_start();

    gpio_pin_configure(gpio1_dev, CPS_PIN, GPIO_OUTPUT);
    gpio_pin_configure(gpio1_dev, CHL_PIN, GPIO_OUTPUT);

    printk("NRF_RADIO->TXPOWER:0x%x\n",NRF_RADIO->TXPOWER);

    for (;;)
    {
        printk("NRF_RADIO->TXPOWER:0x%x\n",NRF_RADIO->TXPOWER);
        gpio_pin_toggle(gpio1_dev, CPS_PIN);
        gpio_pin_toggle(gpio1_dev, CHL_PIN);
        dk_set_led(DK_STATUS_LED, (++blink) % 2);
        k_sleep(K_MSEC(2000));
        gpio_pin_toggle(gpio1_dev, CPS_PIN);
        gpio_pin_toggle(gpio1_dev, CHL_PIN);
        k_sleep(K_MSEC(15));
    }
    return 0;
}

K_THREAD_DEFINE(sens_sample_thread_id, SENS_THREAD_STACK_SIZE, sens_sample_thread, NULL, NULL, NULL,
                SENS_THREAD_PRIORITY, 0, 0);
K_THREAD_DEFINE(ble_write_thread_id, BLE_THREAD_STACK_SIZE, ble_write_thread, NULL, NULL, NULL, BLE_THREAD_PRIORITY, 0,
                0);
