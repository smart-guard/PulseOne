const express = require('express');
const router = express.Router();

router.get('/user', (req, res) => {
  res.json({ name: 'John Doe' });
});

module.exports = router;
