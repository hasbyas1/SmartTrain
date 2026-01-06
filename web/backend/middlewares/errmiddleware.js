// middlewares/errmiddleware.js
const { sequelize } = require('../models');

module.exports = async function autoResync(err, req, res, next) {
  console.log("=== ERROR MIDDLEWARE ===");
  console.log("Error name:", err.name);
  console.log("Error message:", err.message);
  console.log("Original code:", err.original?.code);
  console.log("Original sqlMessage:", err.original?.sqlMessage);
  
  // Kalau bukan error DB, skip
  if (!err?.original?.code) {
    console.log("Not a DB error, passing to next handler");
    return next(err);
  }

  if (err.original.code === 'ER_BAD_FIELD_ERROR') {
    console.warn('[AUTO-RESYNC] Kolom hilang di DB, mencoba sync ulang...');
    try {
      await sequelize.sync({ alter: true });
      console.log('[AUTO-RESYNC] Sync ulang selesai');
      return res.status(500).json({ 
        message: "Database schema updated. Please try again.",
        needRetry: true
      });
    } catch (syncErr) {
      console.error('[AUTO-RESYNC] Gagal sync ulang:', syncErr);
    }
  }

  // Tetap kirim respon error ke user
  return res.status(500).json({ 
    message: err.message || "Internal server error",
    error: err.original?.sqlMessage || err.message
  });
};