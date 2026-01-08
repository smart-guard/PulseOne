// index.js

const config = require('./lib/config/env');  // 먼저 환경변수 검증 및 로딩

const app = require('./app');

const PORT = process.env.PORT || 3000;

app.listen(PORT, '0.0.0.0', () => {
    console.log(`Backend listening on port ${PORT}`);
});
