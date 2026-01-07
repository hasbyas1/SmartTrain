const express = require("express");
const bcrypt = require("bcryptjs");
const jwt = require("jsonwebtoken");
const { sequelize, User } = require("./models");
const cors = require("cors");
const axios = require("axios");
require("dotenv").config();

const authMiddleware = require("./middlewares/authmiddleware");
const autoResync = require("./middlewares/errmiddleware");
const mqttClient = require("./mqtt"); // ✅ Import MQTT Client

const app = express();
const clients = [];

app.use(cors());
app.use(express.json());

// ==========================================
// 🏠 HOME ROUTE
// ==========================================
app.get("/", (req, res) => {
  res.status(201).json({ 
    message: "Halo dari API Smart-Train Unified Backend",
    endpoints: {
      auth: ["/auth/register", "/auth/login", "/auth/user/:id"],
      train: ["/train/latest", "/train/realtime", "/train-speed/history"],
      palang: ["/palang", "/palang/update"],
      camera: ["/camera", "/camera/update"]
    }
  });
});

// ==========================================
// 🔐 AUTHENTICATION ROUTES
// ==========================================

// Register
app.post("/auth/register", async (req, res, next) => {
  console.log("=== REGISTER REQUEST ===");
  console.log("Body received:", req.body);
  
  const { email, name, password } = req.body;
  
  if (!email || !name || !password) {
    return res.status(400).json({ 
      message: "Email, name, and password are required" 
    });
  }
  
  try {
    // Check if email already exists
    const existingUser = await User.findOne({ where: { email } });
    if (existingUser) {
      return res.status(400).json({ message: "Email already registered" });
    }

    console.log("Hashing password...");
    const hashedPassword = await bcrypt.hash(password, 10);
    
    console.log("Creating user with data:", { email, name, hasPassword: !!hashedPassword });
    const newUser = await User.create({ 
      email, 
      name, 
      password: hashedPassword 
    });
    
    console.log("✅ User created successfully! ID:", newUser.id);
    res.status(201).json({ message: "User registered successfully" });
  } catch (error) {
    console.error("❌ REGISTRATION ERROR:");
    console.error("- Error name:", error.name);
    console.error("- Error message:", error.message);
    console.error("- SQL:", error.sql);
    console.error("- Original code:", error.original?.code);
    console.error("- Original errno:", error.original?.errno);
    console.error("- Original sqlMessage:", error.original?.sqlMessage);
    next(error);
  }
});

// Login
app.post("/auth/login", async (req, res, next) => {
  const { email, password } = req.body;
  
  try {
    const user = await User.findOne({ where: { email } });
    
    if (!user) {
      return res.status(401).json({ message: "Email not found" });
    }

    const match = await bcrypt.compare(password, user.password);
    if (!match) {
      return res.status(401).json({ message: "Wrong password" });
    }

    // Generate JWT
    const token = jwt.sign(
      { id: user.id, email: user.email, name: user.name },
      process.env.JWT_SECRET || "SUPER_SECRET_JWT_UBAH_INI",
      { expiresIn: "7d" }
    );

    res.json({
      accessToken: token,
      user: {
        id: user.id,
        name: user.name,
        email: user.email,
      },
    });
  } catch (error) {
    console.error("❌ LOGIN ERROR:", error);
    next(error);
  }
});

// Get User by ID
app.get("/auth/user/:id", async (req, res) => {
  try {
    const user = await User.findByPk(req.params.id, {
      attributes: { exclude: ['password'] }
    });

    if (!user) {
      return res.status(404).json({ message: "User not found" });
    }

    res.json(user);
  } catch (error) {
    console.error("❌ GET USER ERROR:", error);
    res.status(500).json({ message: "Internal server error" });
  }
});

// ==========================================
// 🚆 TRAIN SPEED ROUTES
// ==========================================

// Get Latest Train Speed
app.get("/train/latest", async (req, res) => {
  try {
    const [rows] = await sequelize.query(
      "SELECT * FROM train_speed ORDER BY id DESC LIMIT 1"
    );
    
    if (rows.length === 0) {
      return res.status(404).json({ message: "No data found" });
    }
    
    res.json(rows[0]);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Database error" });
  }
});

// Get Train Speed History with Time Filter
app.get("/train-speed/history", async (req, res) => {
  const { filter } = req.query;

  let condition = "";

  switch (filter) {
    case "1m":
      condition = "created_at >= NOW() - INTERVAL 1 MINUTE AND DATE(created_at) = CURDATE()";
      break;
    case "5m":
      condition = "created_at >= NOW() - INTERVAL 5 MINUTE AND DATE(created_at) = CURDATE()";
      break;
    case "10m":
      condition = "created_at >= NOW() - INTERVAL 10 MINUTE AND DATE(created_at) = CURDATE()";
      break;
    case "30m":
      condition = "created_at >= NOW() - INTERVAL 30 MINUTE AND DATE(created_at) = CURDATE()";
      break;
    default:
      condition = "created_at >= NOW() - INTERVAL 5 MINUTE AND DATE(created_at) = CURDATE()";
  }

  try {
    const [rows] = await sequelize.query(`
      SELECT speed, created_at
      FROM train_speed
      WHERE ${condition}
      ORDER BY created_at ASC
    `);

    res.json(rows);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "DB Error" });
  }
});

// Get Realtime Train Speed
app.get("/train/realtime", async (req, res) => {
  try {
    const [rows] = await sequelize.query(`
      SELECT 
        segment,
        speed,
        DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') AS created_at
      FROM train_speed_realtime
      ORDER BY id DESC
      LIMIT 30
    `);

    res.json(rows.reverse());
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "DB Error" });
  }
});

// ==========================================
// 🚧 PALANG (BARRIER) ROUTES
// ==========================================

// Get Latest Palang Status
app.get("/palang", async (req, res) => {
  try {
    const [rows] = await sequelize.query(
      "SELECT * FROM palang ORDER BY id DESC LIMIT 1"
    );
    
    if (rows.length === 0) {
      return res.status(404).json({ message: "No palang data found" });
    }
    
    // Return as array for Flutter compatibility
    res.json([rows[0]]);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Database error" });
  }
});

// Update Palang Status via MQTT
app.post("/palang/update", async (req, res) => {
  const { status } = req.body;
  
  console.log(`📤 Publishing palang status: ${status}`);
  mqttClient.publish("smartTrain/barrier", JSON.stringify({ status }));
  
  res.json({ success: true });
});

// ==========================================
// 📸 CAMERA ROUTES
// ==========================================

// Get Latest Camera Status
app.get("/camera", async (req, res) => {
  try {
    const [rows] = await sequelize.query(
      "SELECT * FROM camera ORDER BY id DESC LIMIT 1"
    );
    
    if (rows.length === 0) {
      return res.status(404).json({ message: "No camera data found" });
    }
    
    // Return as array for Flutter compatibility
    res.json([rows[0]]);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Database error" });
  }
});

// Update Camera Status via MQTT
app.post("/camera/update", async (req, res) => {
  const { status } = req.body;
  
  console.log(`📤 Publishing camera status: ${status}`);
  mqttClient.publish("smartTrain/camera", JSON.stringify({ status }));
  
  res.json({ success: true });
});

// ==========================================
// 🎥 ESP32-CAM STREAM ROUTE (if needed)
// ==========================================
app.get("/camera/stream", async (req, res) => {
  try {
    const ESP32_CAM_URL = process.env.ESP32_CAM_URL;
    
    if (!ESP32_CAM_URL) {
      return res.status(500).json({ 
        error: "ESP32_CAM_URL not configured in .env" 
      });
    }

    const response = await axios.get(ESP32_CAM_URL, {
      responseType: 'stream'
    });

    res.set('Content-Type', 'multipart/x-mixed-replace; boundary=frame');
    response.data.pipe(res);
  } catch (error) {
    console.error("❌ Camera stream error:", error);
    res.status(500).json({ error: "Failed to fetch camera stream" });
  }
});

// ==========================================
// ❌ ERROR HANDLER MIDDLEWARE
// ==========================================
app.use(autoResync);

// ==========================================
// 🚀 START SERVER
// ==========================================
const PORT = process.env.PORT || 5000;

sequelize
  .authenticate()
  .then(() => {
    console.log("✅ Database connected!");
    
    app.listen(PORT, "0.0.0.0", () => {
      console.log(`🚀 Server running at http://0.0.0.0:${PORT}`);
      console.log("📡 MQTT client initialized and listening...");
    });
  })
  .catch((err) => {
    console.error("❌ Unable to connect to database:", err);
  });