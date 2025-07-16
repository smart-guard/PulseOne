# backend/Dockerfile
FROM node:20

WORKDIR /app/backend

COPY backend/package*.json ./
RUN npm install

# 전체 백엔드 소스
COPY backend /app/backend

# config는 항상 복사
COPY config /app/config

CMD ["node", "app.js"]
