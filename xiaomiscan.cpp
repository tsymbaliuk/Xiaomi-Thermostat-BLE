//
//  Intel Edison Playground
//  Copyright (c) 2015 Damian Kołakowski. All rights reserved.
//
// g++ -std=c++11 -o xiaomiscan xiaomiscan.cpp `pkg-config --cflags ncurses` -lbluetooth -lncurses

#include <stdlib.h>
#include <errno.h>
#include <curses.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

struct hci_request ble_hci_request(uint16_t ocf, int clen, void * status, void * cparam)
{
  struct hci_request rq;
  memset(&rq, 0, sizeof(rq));
  rq.ogf = OGF_LE_CTL;
  rq.ocf = ocf;
  rq.cparam = cparam;
  rq.clen = clen;
  rq.rparam = status;
  rq.rlen = 1;
  return rq;
}

int main()
{
  int ret, status;

  // Get HCI device.

  const int device = hci_open_dev(hci_get_route(NULL));
  if ( device < 0 ) {
    perror("Failed to open HCI device.");
    return 0;
  }

  // Set BLE scan parameters.

  le_set_scan_parameters_cp scan_params_cp;
  memset(&scan_params_cp, 0, sizeof(scan_params_cp));
  scan_params_cp.type             = 0x00;
  scan_params_cp.interval         = htobs(0x0010);
  scan_params_cp.window             = htobs(0x0010);
  scan_params_cp.own_bdaddr_type     = 0x00; // Public Device Address (default).
  scan_params_cp.filter             = 0x00; // Accept all.

  struct hci_request scan_params_rq = ble_hci_request(OCF_LE_SET_SCAN_PARAMETERS, LE_SET_SCAN_PARAMETERS_CP_SIZE, &status, &scan_params_cp);

  ret = hci_send_req(device, &scan_params_rq, 1000);
  if ( ret < 0 ) {
    hci_close_dev(device);
    perror("Failed to set scan parameters data.");
    return 0;
  }

  // Set BLE events report mask.

  le_set_event_mask_cp event_mask_cp;
  memset(&event_mask_cp, 0, sizeof(le_set_event_mask_cp));
  int i = 0;
  for ( i = 0 ; i < 8 ; i++ ) event_mask_cp.mask[i] = 0xFF;

  struct hci_request set_mask_rq = ble_hci_request(OCF_LE_SET_EVENT_MASK, LE_SET_EVENT_MASK_CP_SIZE, &status, &event_mask_cp);
  ret = hci_send_req(device, &set_mask_rq, 1000);
  if ( ret < 0 ) {
    hci_close_dev(device);
    perror("Failed to set event mask.");
    return 0;
  }

  // Enable scanning.

  le_set_scan_enable_cp scan_cp;
  memset(&scan_cp, 0, sizeof(scan_cp));
  scan_cp.enable         = 0x01;    // Enable flag.
  scan_cp.filter_dup     = 0x00; // Filtering disabled.

  struct hci_request enable_adv_rq = ble_hci_request(OCF_LE_SET_SCAN_ENABLE, LE_SET_SCAN_ENABLE_CP_SIZE, &status, &scan_cp);

  ret = hci_send_req(device, &enable_adv_rq, 1000);
  if ( ret < 0 ) {
    hci_close_dev(device);
    perror("Failed to enable scan.");
    return 0;
  }

  // Get Results.

  struct hci_filter nf;
  hci_filter_clear(&nf);
  hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
  hci_filter_set_event(EVT_LE_META_EVENT, &nf);
  if ( setsockopt(device, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0 ) {
    hci_close_dev(device);
    perror("Could not set socket options\n");
    return 0;
  }

  printf("Scanning....\n");

  uint8_t buf[HCI_MAX_EVENT_SIZE];
  evt_le_meta_event * meta_event;
  le_advertising_info * info;
  int len;

  while ( 1 ) {
    len = read(device, buf, sizeof(buf));
    if ( len >= HCI_EVENT_HDR_SIZE ) {
      meta_event = (evt_le_meta_event*)(buf + HCI_EVENT_HDR_SIZE + 1);
      if ( meta_event->subevent == EVT_LE_ADVERTISING_REPORT ) {
        uint8_t reports_count = meta_event->data[0];
        void * offset = meta_event->data + 1;
        while ( reports_count-- ) {
          info = (le_advertising_info *)offset;
          char addr[18];
          ba2str(&(info->bdaddr), addr);
          //                    printf("%s - RSSI %d\n", addr, (char)info->data[info->length]);
          offset = info->data + info->length + 2;
          //                    printf("data: ");
          //                    for (int i = 0; i < info->length; ++i) {
          //                        printf("%02x", (char)info->data[i]);
          //                    }
          //                    printf("\n");
          unsigned char type, length, *value;
          int iter = 0;
          do {
            type = length = -1;
            value = nullptr;
            if ( iter < info->length )
              length = info->data[iter++];
            if ( iter < info->length )
              type = info->data[iter++];
            if ( length != -1 && type != -1 && iter + length - 1 <= info->length ) {
              value = info->data + iter;
              iter += length - 1;
            }
            if ( value ) {
              if ( type == 0x16 ) {
                unsigned short vendor = -1;
                if ( length >= 18 && value[0] == 0x95 && value[1] == 0xFE /* Xiaomi Inc */) {
                  int measure_type = value[13];
                  int measure_length = value[15];
                  unsigned char mac[6] = { value[12], value[11], value[10], value[9], value[8], value[7] };
                  int message_id = value[6];
                  printf("{ \"id\": %d, \"mac\":\"%02x%02x%02x%02x%02x%02x\", \"type\": %d",
                         message_id,
                         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                         measure_type);
                  enum MeasureType {
                    TemperatureHumidity = 0x0D,
                    Battery = 0x0A,
                    Humidity = 0x06,
                    Temperature = 0x04,
                  };
                  if ( measure_type == TemperatureHumidity && measure_length == 4 && length == 21 ) {
                    const int temperature = (int(value[17]) << 8) + value[16];
                    const int humidity    = (int(value[19]) << 8) + value[18];
                    printf(", \"temperature\": %d.%01d, \"humidity\": %d.%d }",
                           temperature / 10, temperature % 10,
                           humidity / 10, humidity % 10);
                  } else if ( measure_type == Battery && measure_length == 1 && length == 18 ) {
                    const int battery = value[16];
                    printf(", \"battery\": %d }", battery);
                  } else if ( measure_type == Humidity && measure_length == 2 && length == 19 ) {
                    const int humidity = (int(value[17]) << 8) + value[16];
                    printf(", \"humidity\": %d.%01d }", humidity / 10, humidity % 10);
                  } else if ( measure_type == Temperature && measure_length == 2 && length == 19 ) {
                    const int temperature = (int(value[17]) << 8) + value[16];
                    printf(", \"temperature\": %d.%01d }", temperature / 10, temperature % 10);
                  } else if ( measure_type == TemperatureHumidity && measure_length == 4 && length == 25 ) {
                    const int temperature = (int(value[17]) << 8) + value[16];
                    const int humidity    = (int(value[19]) << 8) + value[18];
                    const int battery     = value[23];
                    printf(", \"temperature\": %d.%01d, \"humidity\": %d.%d, \"battery\": %d }",
                           temperature / 10, temperature % 10,
                           humidity / 10, humidity % 10,
                           battery);
                  } else if ( measure_type == Humidity && measure_length == 2 && length == 23 ) {
                    const int humidity = (int(value[17]) << 8) + value[16];
                    const int battery  = value[21];
                    printf(", \"humidity\": %d.%01d, \"battery\": %d }", humidity / 10, humidity % 10, battery);
                  } else if ( measure_type == Temperature && measure_length == 2 && length == 23 ) {
                    const int temperature = (int(value[17]) << 8) + value[16];
                    const int battery     = value[21];
                    printf(", \"temperature\": %d.%01d, \"battery\": %d }", temperature / 10, temperature % 10, battery);
                  } else {
                    printf(" }");
                  }
                  printf("\n");
                  fflush(stdout);
                }
              }
            }
          } while ( value != nullptr );
        }
      }
    }
  }

  // Disable scanning.

  memset(&scan_cp, 0, sizeof(scan_cp));
  scan_cp.enable = 0x00;    // Disable flag.

  struct hci_request disable_adv_rq = ble_hci_request(OCF_LE_SET_SCAN_ENABLE, LE_SET_SCAN_ENABLE_CP_SIZE, &status, &scan_cp);
  ret = hci_send_req(device, &disable_adv_rq, 1000);
  if ( ret < 0 ) {
    hci_close_dev(device);
    perror("Failed to disable scan.");
    return 0;
  }

  hci_close_dev(device);

  return 0;
}
