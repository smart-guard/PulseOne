# 운영용 Multi-stage build
FROM node:18-alpine as build

WORKDIR /app

# 패키지 관리자 최적화
RUN npm install -g pnpm

# 패키지 파일들 복사
COPY package*.json ./
COPY pnpm-lock.yaml* ./

# 의존성 설치 (운영용)
RUN pnpm install --frozen-lockfile --prod=false

# 소스 코드 복사 및 빌드
COPY . .
RUN pnpm run build

# Production stage
FROM nginx:alpine

# 빌드된 파일을 nginx로 복사
COPY --from=build /app/dist /usr/share/nginx/html

# Nginx 설정 복사
COPY nginx.conf /etc/nginx/nginx.conf

# 포트 노출
EXPOSE 80

# Nginx 실행
CMD ["nginx", "-g", "daemon off;"]