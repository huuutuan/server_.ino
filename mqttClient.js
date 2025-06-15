const mqtt = require('mqtt');

const url = 'mqtts://3c8284fc593942649ce5a137437254d6.s1.eu.hivemq.cloud:8883'

const options = {
	username: process.env.MQTT_USERNAME,
	password: process.env.MQTT_PASSWORD,
	rejectUnauthorized: false,
}
console.log('MQTT_USERNAME: ', process.env.MQTT_USERNAME);
console.log('MQTT_PASSWORD: ', process.env.MQTT_PASSWORD);

	function createMQTTClient() {
		const client = mqtt.connect(url, options);
		client.on('connect', function () {
			console.log('Connected');
			client.subscribe('waterPump/+/sensor_data', (err) => {
				if (err) console.log('subscribe error: ', err);
				else console.log('subscribe success');
			})
		});
		client.on('error', function (error) {
			console.error('MQTT Connection Error:', error.message);
        		console.error('Details:', error);
		});
		
		return client;
	}

	function publishDeviceDataToMQTT(client, deviceId, data, type = 'sensor_data') {
	  const topic = `waterPump/${deviceId}/${type}`;
	  client.publish(topic, JSON.stringify(data), { qos: 1 }, (err) => {
	    if (err) {
	      console.error('Publish error:', err);
	    } else {
	      console.log(`Published Firestore change to topic: ${topic}`);
	    }
	  });
	}

module.exports = { createMQTTClient, publishDeviceDataToMQTT };

