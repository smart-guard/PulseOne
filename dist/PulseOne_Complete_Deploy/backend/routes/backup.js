const express = require('express');
const router = express.Router();

router.get('/list', async (req, res) => {
  try {
    res.json({
      success: true,
      data: [],
      message: 'Backup Management API - 구현 예정'
    });
  } catch (error) {
    res.status(500).json({ success: false, error: error.message });
  }
});

module.exports = router;
