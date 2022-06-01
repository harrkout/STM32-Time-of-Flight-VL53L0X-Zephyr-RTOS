/*
 * Harry Koutsourelakis 01/06/22
 */

//Golioth's header files
#include <logging/log.h>
LOG_MODULE_REGISTER(golioth_lightdb_stream, LOG_LEVEL_DBG);

#include <net/coap.h>
#include <net/golioth/system_client.h>
#include <net/golioth/wifi.h>

#include <drivers/sensor.h>
#include <stdlib.h>

//VL53L0X header files

#include <zephyr.h>
#include <device.h>
#include <drivers/sensor.h>
#include <stdio.h>
#include <sys/printk.h>

//Enables Golioth client
static struct golioth_client *client = GOLIOTH_SYSTEM_CLIENT_GET();			


void main(void)
{

	const struct device *dev = device_get_binding(DT_LABEL(DT_INST(0, st_vl53l0x)));
	//struct for reading the sensor values (universal library in zephyr, I think?).
	struct sensor_value dist, prox;

	//strings are used to save the data, because Golioth uses CoAP Protocol and can send only Plain Text.
	char str_distance[64];
	char str_proximity[32];
	int ret;

	//Message at the beginning of project (in UART).
	LOG_DBG("Starting LightDB Stream of Time-of-Flight Sensor");

	//Enables Golioth's wifi settings.
	if (IS_ENABLED(CONFIG_GOLIOTH_SAMPLE_WIFI)) {
		LOG_INF("Connecting to WiFi");
		wifi_connect();
	}

	if (dev == NULL) {
		printk("Could not get VL53L0X device\n");
		return;
	}

	//Clients starts.
	golioth_system_client_start();

	//Checks whether the VL53L0X sensor is available.
	while (true) {
		ret = sensor_sample_fetch(dev);
		if (ret) {
			printk("sensor_sample_fetch failed ret %d\n", ret);
			return;
			k_sleep(K_SECONDS(1));
			continue;
		}

		str_distance[sizeof(sensor_value_to_double(&dist)) - 1] = '\0';
		str_proximity[sizeof(prox)] = '\0';


		//Proximity

		//Variable ret saves the state of the data taken from the sensor.
		ret = sensor_channel_get(dev, SENSOR_CHAN_PROX, &prox);
		//Prints in UART the received data.
		printk("========================================================================================\n");
		printk("\n					    ST_VL53L0X Data: \n\n");
		printk("					-------------------------\n", prox);
		printk("					|  Proximity is %d       |\n", prox);
		
		//Distance

		ret = sensor_channel_get(dev, SENSOR_CHAN_DISTANCE, &dist);
		printf("					|  Distance is %.3fm   |\n", sensor_value_to_double(&dist));
		printk("					-------------------------\n", prox);


		//snprintk && snprintf(same usage in Zephyr, I think?) are necessary to send log to LightDB Stream.

		/* USAGE:
		*
		* Composes a string with the same text that would be printed if format was used on printf, but instead of being printed,
		* the content is stored as a C string in the buffer pointed by s (taking n as the maximum buffer capacity to fill).
		*
		*/

		snprintf(str_proximity, sizeof(str_proximity),
			 "%d", prox);

		str_proximity[sizeof(str_proximity) -1 ] = '\0';

		snprintf(str_distance, sizeof(str_distance),

			 "%.3fm", sensor_value_to_double(&dist));
		str_distance[sizeof(str_distance) - 1] = '\0';

		//Message the is shown in UART if Golioth Client is correctly sending data.
		LOG_DBG("Sending Distance %sm\  /|  Proximity %s\n", log_strdup(str_distance), log_strdup(str_proximity));
		printk("================================================================================================\n");


		ret = golioth_lightdb_set(client,
					  GOLIOTH_LIGHTDB_STREAM_PATH("Time-of-Flight: Distance"),
					  COAP_CONTENT_FORMAT_TEXT_PLAIN,
					  str_distance,
					  strlen(str_distance));

		ret = golioth_lightdb_set(client,
					  GOLIOTH_LIGHTDB_STREAM_PATH("Time-of-Flight: Proximity"),
					  COAP_CONTENT_FORMAT_TEXT_PLAIN,
					  str_proximity,
					  strlen(str_proximity));

		if (ret) {
			LOG_WRN("Failed to send data: %d", ret);
		}

		k_sleep(K_MSEC(1000));
	}
}
