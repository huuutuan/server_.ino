const { initializeApp, cert } = require('firebase-admin/app');
const { getFirestore } = require('firebase-admin/firestore');

function initFirestoreFromEnv() {
  initializeApp({
    credential: cert({
      projectId: process.env.PROJECT_ID,
      clientEmail: process.env.CLIENT_EMAIL,
      privateKey: process.env.PRIVATE_KEY.replace(/\\n/g, '\n'),
      
    }),
  });

  return getFirestore();
}

async function saveDeviceData(db, deviceId, payload) {
  const parsed = JSON.parse(payload.toString());
  const timestamp = new Date();

  await db.collection('devices')
    .doc(deviceId)
    .collection('data')
    .add({
      ...parsed,
      timestamp,
    });

  console.log(`Saved data for ${deviceId}`);
}

function listenToDeviceDataChanges(db, onChange) {
  db.collection('devices').onSnapshot(async (snapshot) => {
    snapshot.docChanges().forEach(async (change) => {
      // Lấy deviceId từ document cha của collection 'data'
      const deviceId = change.doc.ref.parent.parent ? change.doc.ref.parent.parent.id : change.doc.ref.id;
      if (change.type === 'added' || change.type === 'modified') {
        const data = change.doc.data();
        onChange(deviceId, data);
      }
    });
  });
}

// Lắng nghe thay đổi status/current và schedule của từng device, tự động phát hiện device mới
function listenToDeviceStatusAndScheduleChanges(db, onStatusChange, onScheduleChange) {
  const listenedDevices = new Set();
  db.collection('devices').onSnapshot(snapshot => {
    // Lắng nghe cho tất cả thiết bị hiện có (chỉ đăng ký nếu chưa từng đăng ký)
    snapshot.forEach(deviceDoc => {
      const deviceId = deviceDoc.id;
      if (listenedDevices.has(deviceId)) return;
      listenedDevices.add(deviceId);
      deviceDoc.ref.collection('status').doc('current').onSnapshot(doc => {
        if (doc.exists) {
          const data = doc.data();
          onStatusChange(deviceId, data);
        }
      });
      deviceDoc.ref.collection('schedule').onSnapshot(scheduleSnapshot => {
        scheduleSnapshot.docChanges().forEach(change => {
          const data = change.doc.data();
          onScheduleChange(deviceId, data, change.type);
        });
      });
    });
    // Lắng nghe thiết bị mới thêm vào (chỉ đăng ký nếu chưa từng đăng ký)
    snapshot.docChanges().forEach(change => {
      if (change.type === 'added') {
        const deviceId = change.doc.id;
        if (listenedDevices.has(deviceId)) return;
        listenedDevices.add(deviceId);
        change.doc.ref.collection('status').doc('current').onSnapshot(doc => {
          if (doc.exists) {
            const data = doc.data();
            onStatusChange(deviceId, data);
          }
        });
        change.doc.ref.collection('schedule').onSnapshot(scheduleSnapshot => {
          scheduleSnapshot.docChanges().forEach(change => {
            const data = change.doc.data();
            onScheduleChange(deviceId, data, change.type);
          });
        });
      }
    });
  });
}

module.exports = { initFirestoreFromEnv, saveDeviceData, listenToDeviceDataChanges, listenToDeviceStatusAndScheduleChanges };
