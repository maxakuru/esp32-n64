#ifndef CONFIGURATOR_H_
#define CONFIGURATOR_H_

#define AP_SSID "esp32-n64"
#define AP_PASS "n64n64n64"


void wifi_config_enable();
void wifi_config_disable();
bool wifi_config_enabled();
void handle_get_index();
void handle_set_value();
void handle_get_value();



#endif // CONFIGURATOR_H_