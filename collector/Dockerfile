# Makefile - PulseOne Collector 빌드 + Docker + CI/CD 확장

CXX = g++
CXXFLAGS = -std=c++17 -Wall \
	-Iinclude \
	-Iinclude/Database \
	-Iinclude/Utils \
	-Iinclude/Plugin \
	-Iinclude/Config \
	-Iinclude/Engine \
	-Iinclude/Publisher \
	-Iinclude/Drivers

LDFLAGS = -lpqxx -lpq -lsqlite3

SRCS = $(shell find src -name '*.cpp')
OBJS = $(SRCS:.cpp=.o)
TARGET = bin/collector

ENV_FILE = ../config/.env
ENV_TEMPLATE = ../config/.env.template

all: precheck $(TARGET)

# 🔹 1. precheck: .env 자동 복사
precheck:
	@if [ ! -f $(ENV_FILE) ]; then \
		echo "⚠️  $(ENV_FILE) not found. Copying from template..."; \
		cp $(ENV_TEMPLATE) $(ENV_FILE); \
	fi

# 🔹 2. 빌드 대상
$(TARGET): $(OBJS)
	@mkdir -p bin
	$(CXX) -o $@ $^ $(LDFLAGS)

# 🔹 3. 정리
clean:
	rm -f $(OBJS) $(TARGET)

# 🔹 4. Docker 빌드 및 실행
build-docker: precheck
	docker build -t pulseone-collector .

run-docker:
	docker run --rm -it \
		-v $(PWD)/../config:/app/config \
		-v $(PWD)/logrotate:/etc/logrotate.d \
		-v $(PWD)/logs:/opt/pulseone/logs \
		--name pulseone-instance \
		pulseone-collector

# 🔹 5. CI/CD용 명령어
ci-check:
	@echo "✅ Running static check..."
	@$(CXX) $(CXXFLAGS) -fsyntax-only $(SRCS)

ci-build: clean all
	@echo "✅ Build success."

ci-test:
	@echo "🧪 No unit tests defined. Skipping..."

ci: ci-check ci-build ci-test

.PHONY: all clean build-docker run-docker precheck ci-check ci-build ci-test ci
