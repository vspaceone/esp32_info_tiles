#define USE_WM
const char* conf_ssid = "esp32_space_info config AP";
const char* conf_pass = "PLEASE_change_me";
//or
//const char* wifi_ssid = "your SSID";
//const char* wifi_pass = "your password";
//#define USE_STATIC_IP
//IPAddress static_ip(10,1,0,53);
//IPAddress gateway(10,0,0,1);
//IPAddress subnet(255,0,0,0);
//IPAddress dns1(10,0,0,1);
//IPAddress dns2(1,1,1,1);

const char * ota_pass = "CHANGE_ME_please";

const char* NTP_SERVER = "pool.ntp.org";

//#define TRANS_FLAG
#define LGBTQ_FLAG
//#define COLOR_BARS

const char * hostname = "info_tiles";

//#define USE_HTTPS_WEBHOOK
const char * bootup_request_data_webhook = "";