#ifndef MQTT_LIB_H
#define MQTT_LIB_H

void mqtt_start(void);
void mqtt_subscribe(char *topic, int qos);
void mqtt_unsubscribe(char *topic);
void mqtt_publish(char *topic, char *payload, int qos, int retain);
int mqtt_connected(void);
void mqtt_disconect(void);

#endif