/***************************************************************************//**
 * @file app.c
 * @brief Fixed Data Bluetooth Advertising (Simulate DHT11 for Testing)
 ******************************************************************************/
#include "em_common.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "gatt_db.h"
#include "app.h"
#include "custom_adv.h"
#include "app_timer.h"
#include "em_gpio.h"
#include "em_cmu.h"
#include "em_core.h"
#include "sl_udelay.h"
#include <stdio.h>

// --- CẤU HÌNH GIÁ TRỊ GIẢ LẬP ---
#define FIXED_TEMP_INT  32  // 25 độ
#define FIXED_TEMP_DEC  5   // .5
#define FIXED_HUM_INT   67  // 60 %
#define FIXED_HUM_DEC   0   // .0

// --- BIẾN TOÀN CỤC ---
CustomAdv_t sData;
static app_timer_t update_timer;
static uint8_t advertising_set_handle = 0xff;

// Biến lưu dữ liệu
uint8_t Rh_byte1, Rh_byte2, Temp_byte1, Temp_byte2;

// --- HÀM PHỤ TRỢ: CHUYỂN ĐỔI SỐ SANG DẠNG VISUAL HEX ---
// Giữ nguyên hàm này để App Flutter hiển thị đúng
// Ví dụ: 25 -> 0x25
uint8_t make_visual_dec(uint8_t val) {
    if (val > 99) val = 99;
    return ((val / 10) << 4) | (val % 10);
}

// --- CALLBACK TIMER: Cập nhật Bluetooth với dữ liệu giả ---
static void update_timer_cb(app_timer_t *timer, void *data)
{
  (void)data; (void)timer;

  // 1. GÁN DỮ LIỆU CỐ ĐỊNH (Bỏ qua bước đọc cảm biến)
  // Không gọi DHT11_Start() hay CORE_ENTER_CRITICAL() để tránh lỗi phần cứng

  Temp_byte1 = FIXED_TEMP_INT;
  Temp_byte2 = FIXED_TEMP_DEC;
  Rh_byte1   = FIXED_HUM_INT;
  Rh_byte2   = FIXED_HUM_DEC;

  // 2. IN LOG KIỂM TRA (Qua cổng COM/UART)
  printf("[GIA LAP] Do am: %d.%d %% | Nhiet do: %d.%d C\r\n",
         Rh_byte1, Rh_byte2, Temp_byte1, Temp_byte2);

  // 3. CẬP NHẬT BLUETOOTH
  // Chuyển sang Visual Hex (VD: 25 -> 0x25) để khớp với thuật toán App Flutter
  uint8_t v_temp_int = make_visual_dec(Temp_byte1);
  uint8_t v_temp_dec = make_visual_dec(Temp_byte2);
  uint8_t v_hum_int  = make_visual_dec(Rh_byte1);
  uint8_t v_hum_dec  = make_visual_dec(Rh_byte2);

  // Đóng gói: [T.Int][T.Dec][H.Int][H.Dec]
  // Ví dụ 25.5C, 60.0% -> Sẽ gửi đi chuỗi Hex: 25 05 60 00
  uint32_t sensor_data_packed = (v_temp_int << 24) |
                                (v_temp_dec << 16) |
                                (v_hum_int  << 8)  |
                                v_hum_dec;

  // Cập nhật gói tin quảng bá
  update_adv_data(&sData, advertising_set_handle, sensor_data_packed);
}

// --- APP INIT ---
SL_WEAK void app_init(void)
{
  sl_status_t sc;
  printf("\r\n--- HE THONG GIA LAP DU LIEU (FIXED DATA) ---\r\n");

  // Khởi động Timer: 100ms cập nhật 1 lần (Để App Flutter cập nhật RSSI mượt hơn)
  // Nếu bạn muốn chậm hơn thì sửa 100 thành 1000 (1 giây)
  sc = app_timer_start(&update_timer, 100, update_timer_cb, NULL, true);
  app_assert_status(sc);
}

SL_WEAK void app_process_action(void) {}

// --- BLUETOOTH EVENTS ---
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;
  bd_addr address;
  uint8_t address_type;

  switch (SL_BT_MSG_ID(evt->header)) {
    case sl_bt_evt_system_boot_id:
      // Lấy địa chỉ MAC
      sc = sl_bt_system_get_identity_address(&address, &address_type);
      app_assert_status(sc);

      // Tạo Advertising Set
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      // Cấu hình thời gian quảng bá (100ms)
      // Min = 160 * 0.625ms = 100ms
      // Max = 160 * 0.625ms = 100ms
      sc = sl_bt_advertiser_set_timing(advertising_set_handle, 160, 160, 0, 0);
      app_assert_status(sc);

      // Cấu hình kênh
      sl_bt_advertiser_set_channel_map(advertising_set_handle, 7);
      app_assert_status(sc);

      // Tạo dữ liệu ban đầu
      // Tên thiết bị vẫn là IoT-DHT11 để App nhận ra
      fill_adv_packet(&sData, FLAG, COMPANY_ID, 0x00000000, "IoT-DHT11");

      // Bắt đầu quảng bá
      start_adv(&sData, advertising_set_handle);
      printf("Bluetooth da san sang (Mode: Fixed Data).\r\n");
      break;

    case sl_bt_evt_connection_closed_id:
      // Quảng bá lại ngay lập tức nếu bị ngắt kết nối
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle, sl_bt_advertiser_general_discoverable);
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle, sl_bt_advertiser_connectable_scannable);
      app_assert_status(sc);
      break;

    default:
      break;
  }
}
