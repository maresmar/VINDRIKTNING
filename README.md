# My ESP-VINDRIKTNING

Created for LASKAKIT ESP-VINDRIKTNING board, based on https://github.com/LaskaKit/ESP-Vindriktning

## Configuration

The `env.h` file is needed with following content:

```
#define WIFI_ESSID    "MyWiFi"
#define WIFI_PASSWORD "secretpassword"
#define HOSTNAME      "VINDRIKTNING"

#define BROKER_HOST "192.168.0.1"
#define BROKER_PORT 1883
#define MQTT_PREFIX "home/room"

#define INFLUXDB_URL "https://192.168.0.1:8090"
#define INFLUXDB_TOKEN "token"
#define INFLUXDB_ORG "myorg"
#define INFLUXDB_BUCKET "me/probe"
```
