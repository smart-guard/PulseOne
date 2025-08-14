const express = require('express');
const router = express.Router();

router.get('/search', async (req, res) => {
  try {
    res.json({
      success: true,
      data: [],
      message: 'Data Explorer API - 구현 예정'
    });
  } catch (error) {
    res.status(500).json({ success: false, error: error.message });
  }
});

module.exports = router;
