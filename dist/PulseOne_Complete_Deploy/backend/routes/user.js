const express = require('express');
const router = express.Router();

router.get('/', (req, res) => {
    res.json({
        success: true,
        data: [
            { id: 1, name: "Admin User", role: "admin" },
            { id: 2, name: "Test User", role: "user" }
        ]
    });
});

module.exports = router;
