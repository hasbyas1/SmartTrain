// ==========================================
// 🔧 Konfigurasi MQTT HiveMQ Cloud
// ==========================================
const mqtt = require("mqtt");
const { sequelize } = require("./models");

// MQTT Broker Configuration
const mqttServer = "mqtts://9e108cb03c734f0394b0f0b49508ec1e.s1.eu.hivemq.cloud:8883";
const mqttUser = "Device02";
const mqttPass = "Device02";

// Topics
const topicSpeed = "smartTrain/speedometer";
const topicPalang = "smartTrain/barrier";
const topicCamera = "smartTrain/camera";
const topicTelemetry = "smartTrain/telemetry_batch";

// ==========================================
// 🧠 Realtime Dedup Cache
// ==========================================
const lastRealtimeCache = new Map();

// ==========================================
// 🛡️ Queue System & Processing Flags
// ==========================================
let palangQueue = Promise.resolve();
let cameraQueue = Promise.resolve();

let isPalangProcessing = false;
let isCameraProcessing = false;

let lastPalangStatus = null;
let lastCameraStatus = null;

// ==========================================
// 🚀 MQTT Connect
// ==========================================
const mqttClient = mqtt.connect(mqttServer, {
  username: mqttUser,
  password: mqttPass,
  reconnectPeriod: 5000,
});

mqttClient.on("connect", () => {
  console.log("📡 Terhubung ke HiveMQ!");
  mqttClient.subscribe(
    [topicSpeed, topicPalang, topicCamera, topicTelemetry],
    (err) => {
      if (!err) {
        console.log("✅ Subscribe berhasil:");
        console.log(" - " + topicSpeed);
        console.log(" - " + topicPalang);
        console.log(" - " + topicCamera);
        console.log(" - " + topicTelemetry);
      }
    }
  );
});

// ==========================================
// 📥 MQTT Message Handler
// ==========================================
mqttClient.on("message", async (topic, message) => {
  try {
    const data = JSON.parse(message.toString());

    // ======================================================
    // 🚆 KECEPATAN RATA-RATA
    // ======================================================
    if (topic === topicSpeed) {
      const timestamp = new Date();

      if (data.hasOwnProperty("kecepatan_r")) {
        console.log(`📥 RATA-RATA diterima → ${data.kecepatan_r} km/jam`);
        console.log(`⏱️  Waktu total: ${data.waktu_total || "N/A"} detik`);

        const sql = `
          INSERT INTO train_speed (speed, created_at)
          VALUES (?, ?)
        `;

        try {
          await sequelize.query(sql, {
            replacements: [data.kecepatan_r, timestamp],
          });
          console.log("💾 RATA-RATA tersimpan ke DB!");
        } catch (err) {
          console.error("❌ Error insert rata-rata:", err);
        }
      } else if (data.hasOwnProperty("kecepatan_s")) {
        console.log(
          `📊 Segmen ${data.id} → ${data.kecepatan_s} km/jam (Realtime UI only)`
        );
      } else {
        console.log("⚠️ Data format tidak dikenal:", data);
      }

      return;
    }

    // ======================================================
    // 🚆 KECEPATAN REALTIME (STRONG DEDUP)
    // ======================================================
    if (topic === topicTelemetry) {
      let payload;
      try {
        payload = JSON.parse(message.toString());
      } catch {
        return;
      }

      if (!payload.speed || typeof payload.speed !== "object") return;

      const secondBucket = Math.floor(Date.now() / 1000);
      const createdAt = new Date(secondBucket * 1000);

      const inserts = [];

      for (const [segment, speedRaw] of Object.entries(payload.speed)) {
        const speed = Number(speedRaw);
        if (Number.isNaN(speed)) continue;

        const cacheKey = `${segment}_${secondBucket}`;

        if (lastRealtimeCache.has(cacheKey)) continue;

        lastRealtimeCache.set(cacheKey, true);
        inserts.push([segment, speed, createdAt]);
      }

      if (inserts.length === 0) return;

      try {
        await sequelize.query(
          `INSERT INTO train_speed_realtime (segment, speed, created_at) VALUES ?`,
          {
            replacements: [inserts],
          }
        );

        console.log("📈 Telemetry saved:", inserts.length);
      } catch (err) {
        console.error("❌ Telemetry insert error:", err);
      }

      return;
    }

    // ======================================================
    // 🚧 PALANG → QUEUE SYSTEM
    // ======================================================
    if (topic === topicPalang) {
      const currentStatus = data.status;

      console.log(`📥 PALANG request received: ${currentStatus}`);

      if (isPalangProcessing) {
        console.log(`⚠️ PALANG: Already processing, request IGNORED`);
        return;
      }

      palangQueue = palangQueue.then(async () => {
        isPalangProcessing = true;

        try {
          const timestamp = new Date();
          await new Promise((resolve) => setTimeout(resolve, 50));

          if (currentStatus === lastPalangStatus) {
            console.log(
              `⚠️ PALANG: Status sama dengan cache (${currentStatus}), SKIP`
            );
            return;
          }

          const [lastRecord] = await sequelize.query(
            "SELECT status FROM palang ORDER BY id DESC LIMIT 1 FOR UPDATE"
          );

          if (
            lastRecord.length > 0 &&
            lastRecord[0].status === currentStatus
          ) {
            console.log(
              `⚠️ PALANG: Status sama dengan DB (${currentStatus}), SKIP`
            );
            lastPalangStatus = currentStatus;
            return;
          }

          console.log(
            `🚧 PALANG: Insert status ${currentStatus} at ${timestamp.toISOString()}`
          );

          await sequelize.query(
            "INSERT INTO palang (status, created_at, updated_at) VALUES (?, ?, ?)",
            {
              replacements: [currentStatus, timestamp, timestamp],
            }
          );

          console.log("💾 PALANG inserted successfully!");
          lastPalangStatus = currentStatus;
        } catch (err) {
          console.error("❌ PALANG queue error:", err);
        } finally {
          isPalangProcessing = false;
        }
      });

      return;
    }

    // ======================================================
    // 📸 CAMERA → QUEUE SYSTEM
    // ======================================================
    if (topic === topicCamera) {
      const currentStatus = data.status;

      console.log(`📥 CAMERA request received: ${currentStatus}`);

      if (isCameraProcessing) {
        console.log(`⚠️ CAMERA: Already processing, request IGNORED`);
        return;
      }

      cameraQueue = cameraQueue.then(async () => {
        isCameraProcessing = true;

        try {
          const timestamp = new Date();
          await new Promise((resolve) => setTimeout(resolve, 50));

          if (currentStatus === lastCameraStatus) {
            console.log(
              `⚠️ CAMERA: Status sama dengan cache (${currentStatus}), SKIP`
            );
            return;
          }

          const [lastRecord] = await sequelize.query(
            "SELECT status FROM camera ORDER BY id DESC LIMIT 1 FOR UPDATE"
          );

          if (
            lastRecord.length > 0 &&
            lastRecord[0].status === currentStatus
          ) {
            console.log(
              `⚠️ CAMERA: Status sama dengan DB (${currentStatus}), SKIP`
            );
            lastCameraStatus = currentStatus;
            return;
          }

          console.log(
            `📸 CAMERA: Insert status ${currentStatus} at ${timestamp.toISOString()}`
          );

          await sequelize.query(
            "INSERT INTO camera (status, created_at, updated_at) VALUES (?, ?, ?)",
            {
              replacements: [currentStatus, timestamp, timestamp],
            }
          );

          console.log("💾 CAMERA inserted successfully!");
          lastCameraStatus = currentStatus;
        } catch (err) {
          console.error("❌ CAMERA queue error:", err);
        } finally {
          isCameraProcessing = false;
        }
      });

      return;
    }
  } catch (err) {
    console.error("⚠️ Error parsing MQTT message:", err);
    console.error("Topic:", topic);
    console.error("Message:", message.toString());
  }
});

// ==========================================
// 🔌 Error & Close Handlers
// ==========================================
mqttClient.on("error", (err) => {
  console.error("❌ MQTT Error:", err);
});

mqttClient.on("close", () => {
  console.log("🔌 MQTT Connection closed");
});

module.exports = mqttClient;