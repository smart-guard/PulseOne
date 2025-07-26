#!/bin/bash
# 기존 파일에서 문제 부분만 수정

SCRIPT_FILE="backend/scripts/build-sqlite-schema.sh"
TEMP_FILE="temp_build_script.sh"

# 문제가 있는 HERE 문서 부분을 찾아서 수정
sed '/sqlite3 "$DB_FILE" << EOF/,/^EOF$/c\
# SQLite 명령 파일 생성\
SQL_COMMANDS_FILE="$DB_DIR/build_commands.sql"\
cat > "$SQL_COMMANDS_FILE" << '"'"'END_OF_SQL'"'"'\
PRAGMA foreign_keys = ON;\
PRAGMA journal_mode = WAL;\
PRAGMA synchronous = NORMAL;\
\
.read '"'"'$SCHEMA_DIR/01-core-tables.sql'"'"'\
.read '"'"'$SCHEMA_DIR/02-users-sites.sql'"'"'\
.read '"'"'$SCHEMA_DIR/03-device-tables.sql'"'"'\
.read '"'"'$SCHEMA_DIR/04-virtual-points.sql'"'"'\
.read '"'"'$SCHEMA_DIR/05-alarm-tables.sql'"'"'\
.read '"'"'$SCHEMA_DIR/06-log-tables.sql'"'"'\
.read '"'"'$SCHEMA_DIR/07-indexes.sql'"'"'\
\
.read '"'"'$DATA_DIR/initial-data.sql'"'"'\
\
.tables\
END_OF_SQL\
\
# SQLite 실행\
echo -e "${BLUE}⚙️ SQLite 실행 중...${NC}"\
if sqlite3 "$DB_FILE" < "$SQL_COMMANDS_FILE"; then\
    echo -e "${GREEN}✅ 스키마 빌드 완료!${NC}"\
else\
    echo -e "${RED}❌ 스키마 빌드 실패${NC}"\
    rm -f "$SQL_COMMANDS_FILE"\
    exit 1\
fi\
\
# 임시 파일 정리\
rm -f "$SQL_COMMANDS_FILE"' "$SCRIPT_FILE" > "$TEMP_FILE"

# 수정된 파일로 교체
mv "$TEMP_FILE" "$SCRIPT_FILE"
chmod +x "$SCRIPT_FILE"

echo "✅ 스크립트 수정 완료"
