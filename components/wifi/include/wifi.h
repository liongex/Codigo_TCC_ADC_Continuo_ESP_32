#ifndef WIFI_H
#define WIFI_H

void wifi_init_sta(void);
void wifi_ini_ap(const char *ssid, const char *pass);
void wifi_disconnect(void);
int wifi_conect_status();

#endif