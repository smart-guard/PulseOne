const express = require('express');
const router = express.Router();

router.get('/metrics', async (req, res) => {
  try {
    res.json({
      success: true,
      data: {
        cpu_usage: Math.floor(Math.random() * 100),
        memory_usage: Math.floor(Math.random() * 100),
        disk_usage: Math.floor(Math.random() * 100),
        timestamp: new Date().toISOString()
      },
      message: 'System Monitoring API - 구현 예정'
    });
  } catch (error) {
    res.status(500).json({ success: false, error: error.message });
  }
});

module.exports = router;
