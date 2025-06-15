const dotenv = require('dotenv');
dotenv.config();
const express = require('express');
const mqtt = require('mqtt');
const bodyParser = require('body-parser');
const { initFirestoreFromEnv, saveDeviceData, listenToDeviceDataChanges, listenToDeviceStatusAndScheduleChanges } = require('./fireStoreService.js');
const { createMQTTClient, publishDeviceDataToMQTT } = require('./mqttClient.js');


const db = initFirestoreFromEnv();

const mqttClient = createMQTTClient();

mqttClient.on('message', async (topic, payload) => {
	const match = topic.match(/waterPump\/([^/]+)\/sensor_data/);
	const match2 = topic.match(/waterPump\/([^/]+)\/sensor_data/);
	if(!match) {
		return;
	}else {
		const deviceId = match[1];
		console.log(deviceId);
		
		try {
			await saveDeviceData(db, deviceId, payload);
		}
		catch (error) {
			console.error('Error saving data to Firestore:', error);
		}
	}
	

	
})

const app = express();
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));

app.get('/', (req, res) => {
	res.send('Hello World!');
}
);

app.listen(process.env.PORT, () => {
	console.log(`Server is running on port ${process.env.PORT}`);
}
);

// Lắng nghe realtime Firestore và publish lên MQTT khi có thay đổi
// listenToDeviceDataChanges(db, (deviceId, data) => {
//   publishDeviceDataToMQTT(mqttClient, deviceId, data);
// });

// Lắng nghe thay đổi status/current và schedule, publish lên MQTT broker

const lastPublishedStatus = new Map();
const lastPublishedSchedule = new Map();

listenToDeviceStatusAndScheduleChanges(db,
  (deviceId, statusData) => {
    // Publish status/current lên topic waterPump/<deviceId>/status
    const prev = lastPublishedStatus.get(deviceId);
    if (!prev || JSON.stringify(prev) !== JSON.stringify(statusData)){
	    publishDeviceDataToMQTT(mqttClient, deviceId, statusData, 'status');
	    lastPublishedStatus.set(deviceId, statusData);
	}
  },
  (deviceId, scheduleData, changeType) => {
    // Publish schedule lên topic waterPump/<deviceId>/schedule
    const key = deviceId + ':' + JSON.stringify(scheduleData);
    const prev = lastPublishedSchedule.get(key);
    if (!prev || changeType === 'added' || changeType === 'modified') {
      publishDeviceDataToMQTT(mqttClient, deviceId, scheduleData, 'schedule');
      lastPublishedSchedule.set(key, scheduleData);
    }
  }
);


