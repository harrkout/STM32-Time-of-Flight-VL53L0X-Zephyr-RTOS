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
	struct sensor_value value;

	//strings are used to save the data, because Golioth uses CoAP Protocol and can send only Plain Text.
	char str_distance[32];
	char str_proximity[32];
	int err;

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
		err = sensor_sample_fetch(dev);
		if (err) {
			printk("sensor_sample_fetch failed ret %d\n", err);
			return;
			k_sleep(K_SECONDS(1));
			continue;
		}

		str_distance[sizeof(sensor_value_to_double(&value)) - 1] = '\0';
		str_proximity[sizeof(value.val1)] = '\0';


		//Proximity

		//Variable err saves the state of the data taken from the sensor.
		err = sensor_channel_get(dev, SENSOR_CHAN_PROX, &value);
		//Prints in UART the received data.
		printk("Proximity is %d\n", value.val1);
		
		//Distance

		err = sensor_channel_get(dev, SENSOR_CHAN_DISTANCE, &value);
		printf("Distance is %.3fm\n", sensor_value_to_double(&value));

		//snprintk && snprintf(same usage in Zephyr, I think?) are necessary to send log to LightDB Stream.

		/* USAGE:
		*
		* Composes a string with the same text that would be printed if format was used on printf, but instead of being printed,
		* the content is stored as a C string in the buffer pointed by s (taking n as the maximum buffer capacity to fill).
		*
		*/

		snprintf(str_proximity, sizeof(str_proximity),
			 "%d", value.val1);

		str_proximity[sizeof(str_proximity) -1 ] = '\0';

		snprintf(str_distance, sizeof(str_distance),

			 "%.3fm", sensor_value_to_double(&value));
		str_distance[sizeof(str_distance) - 1] = '\0';

		//Message the is shown in UART if Golioth Client is correctly sending data.
		LOG_DBG("Sending Distance %sm\ /|  Proximity %s\n", log_strdup(str_distance), log_strdup(str_proximity));
        //LOG_DBG("Proximity: %d;", sensor_value_to_double(&value.val1));


		err = golioth_lightdb_set(client,
					  GOLIOTH_LIGHTDB_STREAM_PATH("Time-of-Flight: Distance"),
					  COAP_CONTENT_FORMAT_TEXT_PLAIN,
					  str_distance,
					  strlen(str_distance));

		err = golioth_lightdb_set(client,
					  GOLIOTH_LIGHTDB_STREAM_PATH("Time-of-Flight: Proximity"),
					  COAP_CONTENT_FORMAT_TEXT_PLAIN,
					  str_proximity,
					  strlen(str_proximity));

		if (err) {
			LOG_WRN("Failed to send data: %d", err);
		}

		k_sleep(K_MSEC(1000));
	}
}
