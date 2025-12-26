// middlewares/autoResync.js
const { sequelize } = require('../models');

module.exports = async function autoResync(err, req, res, next) {
  // Kalau bukan error DB, skip
  if (!err?.original?.code) {
    return next(err);
  }

  if (err.original.code === 'ER_BAD_FIELD_ERROR') {
    console.warn('[AUTO-RESYNC] Kolom hilang di DB, mencoba sync ulang...');
    try {
      await sequelize.sync({ alter: true });
      console.log('[AUTO-RESYNC] Sync ulang selesai');
    } catch (syncErr) {
      console.error('[AUTO-RESYNC] Gagal sync ulang:', syncErr);
    }
  }

  // Tetap kirim respon error ke user
  return res.status(500).json({ message: err.message, error: err });
};
