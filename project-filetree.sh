#!/bin/bash
# ============================================================================
# Project Tree Analyzer - 프로젝트 구조 분석 도구
# 불필요한 파일들을 제외하고 깔끔한 프로젝트 구조를 생성합니다
# ============================================================================

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
PURPLE='\033[0;35m'
NC='\033[0m' # No Color

# 기본 설정
OUTPUT_DIR="."
SHOW_PREVIEW=true
SAVE_FILES=true
MAX_DEPTH=10

# 도움말 함수
show_help() {
    cat << EOF
🌳 Project Tree Analyzer v1.0

사용법: $0 [옵션]

옵션:
    -o, --output DIR        출력 디렉토리 (기본: 현재 디렉토리)
    -d, --depth NUM         최대 깊이 (기본: 10, overview는 3)
    -p, --no-preview        미리보기 비활성화
    -s, --no-save          파일 저장 비활성화 (화면 출력만)
    -q, --quiet            조용한 모드 (진행 메시지 없음)
    -h, --help             이 도움말 표시

사용 예시:
    $0                      # 기본 실행
    $0 -o docs/             # docs 디렉토리에 저장
    $0 -d 5 -p              # 깊이 5, 미리보기 없음
    $0 -s                   # 파일 저장 없이 화면 출력만

생성되는 파일:
    📄 overview.txt         # 3단계 깊이 개요
    📄 structure.txt        # 전체 상세 구조
    📄 headers.txt          # 헤더 파일만
    📄 sources.txt          # 소스 파일만
    📄 configs.txt          # 설정 파일만

EOF
}

# 로그 함수들
log_info() {
    [[ "$QUIET" != "true" ]] && echo -e "${CYAN}[INFO]${NC} $1"
}

log_success() {
    [[ "$QUIET" != "true" ]] && echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warn() {
    [[ "$QUIET" != "true" ]] && echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

# tree 명령어 존재 확인
check_dependencies() {
    if ! command -v tree &> /dev/null; then
        log_error "tree 명령어가 설치되지 않았습니다"
        echo "설치 방법:"
        echo "  Ubuntu/Debian: sudo apt-get install tree"
        echo "  CentOS/RHEL:   sudo yum install tree"
        echo "  macOS:         brew install tree"
        exit 1
    fi
}

# 출력 디렉토리 생성
setup_output_dir() {
    if [[ "$SAVE_FILES" == "true" ]]; then
        mkdir -p "$OUTPUT_DIR"
        if [[ ! -w "$OUTPUT_DIR" ]]; then
            log_error "출력 디렉토리에 쓰기 권한이 없습니다: $OUTPUT_DIR"
            exit 1
        fi
    fi
}

# 공통 제외 패턴
EXCLUDE_PATTERN="*.o|*.a|*.so|*.dll|*.exe|build*|bin*|.git*|.vscode|.idea|*.swp|*.swo|*~|.DS_Store|Thumbs.db|*.log|logs*|data*|*.db|*.sqlite|*.tmp|*.temp|core|__pycache__|*.pyc|node_modules|CMakeFiles|cmake_install.cmake|CMakeCache.txt|*.backup|.env*"

# 1. 전체 구조 개요 생성 (3단계 깊이)
generate_overview() {
    log_info "📋 전체 구조 개요 생성 중..."
    
    local output_file="$OUTPUT_DIR/overview.txt"
    local temp_file=$(mktemp)
    
    # 헤더 추가
    cat > "$temp_file" << EOF
# ============================================================================
# PulseOne Project Overview (3-Level Structure)
# Generated: $(date '+%Y-%m-%d %H:%M:%S')
# ============================================================================

EOF
    
    # tree 실행
    tree -L 3 -I "$EXCLUDE_PATTERN" >> "$temp_file" 2>/dev/null
    
    if [[ "$SAVE_FILES" == "true" ]]; then
        mv "$temp_file" "$output_file"
        log_success "개요 파일 생성: $output_file"
    else
        cat "$temp_file"
        rm -f "$temp_file"
    fi
}

# 2. 상세 구조 생성
generate_detailed_structure() {
    log_info "📂 상세 구조 생성 중..."
    
    local output_file="$OUTPUT_DIR/structure.txt"
    local temp_file=$(mktemp)
    
    # 헤더 추가
    cat > "$temp_file" << EOF
# ============================================================================
# PulseOne Project Detailed Structure
# Generated: $(date '+%Y-%m-%d %H:%M:%S')
# Max Depth: $MAX_DEPTH
# ============================================================================

EOF
    
    # tree 실행
    if [[ "$MAX_DEPTH" -gt 0 ]]; then
        tree -L "$MAX_DEPTH" -I "$EXCLUDE_PATTERN" >> "$temp_file" 2>/dev/null
    else
        tree -I "$EXCLUDE_PATTERN" >> "$temp_file" 2>/dev/null
    fi
    
    if [[ "$SAVE_FILES" == "true" ]]; then
        mv "$temp_file" "$output_file"
        log_success "상세 구조 파일 생성: $output_file"
    else
        cat "$temp_file"
        rm -f "$temp_file"
    fi
}

# 3. 헤더 파일 구조
generate_headers_structure() {
    log_info "📄 헤더 파일 구조 생성 중..."
    
    local output_file="$OUTPUT_DIR/headers.txt"
    local temp_file=$(mktemp)
    
    cat > "$temp_file" << EOF
# ============================================================================
# PulseOne Project Headers Structure
# Generated: $(date '+%Y-%m-%d %H:%M:%S')
# ============================================================================

EOF
    
    if [[ -d "include" ]]; then
        echo "📁 Include Directory:" >> "$temp_file"
        tree include/ -P "*.h|*.hpp|*.hxx" -I "$EXCLUDE_PATTERN" >> "$temp_file" 2>/dev/null
        echo "" >> "$temp_file"
    fi
    
    # 소스 디렉토리의 헤더들도 포함
    if [[ -d "src" ]]; then
        echo "📁 Source Directory Headers:" >> "$temp_file"
        tree src/ -P "*.h|*.hpp|*.hxx" -I "$EXCLUDE_PATTERN" >> "$temp_file" 2>/dev/null
    fi
    
    if [[ "$SAVE_FILES" == "true" ]]; then
        mv "$temp_file" "$output_file"
        log_success "헤더 구조 파일 생성: $output_file"
    else
        cat "$temp_file"
        rm -f "$temp_file"
    fi
}

# 4. 소스 파일 구조
generate_sources_structure() {
    log_info "💻 소스 파일 구조 생성 중..."
    
    local output_file="$OUTPUT_DIR/sources.txt"
    local temp_file=$(mktemp)
    
    cat > "$temp_file" << EOF
# ============================================================================
# PulseOne Project Sources Structure
# Generated: $(date '+%Y-%m-%d %H:%M:%S')
# ============================================================================

EOF
    
    if [[ -d "src" ]]; then
        echo "📁 Source Directory:" >> "$temp_file"
        tree src/ -P "*.cpp|*.c|*.cc|*.cxx" -I "$EXCLUDE_PATTERN" >> "$temp_file" 2>/dev/null
    fi
    
    if [[ "$SAVE_FILES" == "true" ]]; then
        mv "$temp_file" "$output_file"
        log_success "소스 구조 파일 생성: $output_file"
    else
        cat "$temp_file"
        rm -f "$temp_file"
    fi
}

# 5. 설정 파일 구조
generate_configs_structure() {
    log_info "⚙️  설정 파일 구조 생성 중..."
    
    local output_file="$OUTPUT_DIR/configs.txt"
    local temp_file=$(mktemp)
    
    cat > "$temp_file" << EOF
# ============================================================================
# PulseOne Project Configuration Files
# Generated: $(date '+%Y-%m-%d %H:%M:%S')
# ============================================================================

EOF
    
    tree -P "*.json|*.yml|*.yaml|*.conf|*.cfg|*.ini|*.toml|Makefile|CMakeLists.txt|Dockerfile*|*.env.example|package.json|*.xml" -I "$EXCLUDE_PATTERN" >> "$temp_file" 2>/dev/null
    
    if [[ "$SAVE_FILES" == "true" ]]; then
        mv "$temp_file" "$output_file"
        log_success "설정 파일 구조 생성: $output_file"
    else
        cat "$temp_file"
        rm -f "$temp_file"
    fi
}

# 미리보기 출력
show_preview() {
    if [[ "$SHOW_PREVIEW" == "true" && "$SAVE_FILES" == "true" ]]; then
        echo ""
        echo -e "${PURPLE}📁 Overview Preview:${NC}"
        echo -e "${BLUE}===================${NC}"
        head -25 "$OUTPUT_DIR/overview.txt" | tail -20
        
        echo ""
        echo -e "${PURPLE}📂 Detailed Structure Preview:${NC}"
        echo -e "${BLUE}==============================${NC}"
        head -35 "$OUTPUT_DIR/structure.txt" | tail -25
        
        echo ""
        echo -e "${GREEN}💡 전체 내용을 보려면:${NC}"
        echo "   cat $OUTPUT_DIR/overview.txt"
        echo "   cat $OUTPUT_DIR/structure.txt"
        echo "   cat $OUTPUT_DIR/headers.txt"
        echo "   cat $OUTPUT_DIR/sources.txt"
        echo "   cat $OUTPUT_DIR/configs.txt"
    fi
}

# 통계 정보 출력
show_statistics() {
    if [[ "$SAVE_FILES" == "true" ]]; then
        echo ""
        echo -e "${CYAN}📊 생성된 파일 통계:${NC}"
        echo -e "${BLUE}==================${NC}"
        
        for file in overview.txt structure.txt headers.txt sources.txt configs.txt; do
            if [[ -f "$OUTPUT_DIR/$file" ]]; then
                local lines=$(wc -l < "$OUTPUT_DIR/$file")
                local size=$(du -h "$OUTPUT_DIR/$file" | cut -f1)
                printf "   %-15s: %4d lines, %s\n" "$file" "$lines" "$size"
            fi
        done
    fi
}

# 매개변수 파싱
parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -o|--output)
                OUTPUT_DIR="$2"
                shift 2
                ;;
            -d|--depth)
                MAX_DEPTH="$2"
                shift 2
                ;;
            -p|--no-preview)
                SHOW_PREVIEW=false
                shift
                ;;
            -s|--no-save)
                SAVE_FILES=false
                shift
                ;;
            -q|--quiet)
                QUIET=true
                shift
                ;;
            -h|--help)
                show_help
                exit 0
                ;;
            *)
                log_error "알 수 없는 옵션: $1"
                show_help
                exit 1
                ;;
        esac
    done
}

# 메인 함수
main() {
    # 배너 출력
    if [[ "$QUIET" != "true" ]]; then
        echo -e "${CYAN}"
        echo "🌳 ============================================= 🌳"
        echo "   PulseOne Project Tree Analyzer v1.0"
        echo "   프로젝트 구조 분석 및 문서화 도구"
        echo "🌳 ============================================= 🌳"
        echo -e "${NC}"
    fi
    
    # 의존성 확인
    check_dependencies
    
    # 출력 디렉토리 설정
    setup_output_dir
    
    # 구조 파일들 생성
    generate_overview
    generate_detailed_structure
    generate_headers_structure
    generate_sources_structure
    generate_configs_structure
    
    # 미리보기 출력
    show_preview
    
    # 통계 출력
    show_statistics
    
    if [[ "$QUIET" != "true" ]]; then
        echo ""
        echo -e "${GREEN}🎉 프로젝트 구조 분석 완료!${NC}"
        
        if [[ "$SAVE_FILES" == "true" ]]; then
            echo -e "${YELLOW}📁 출력 위치: $OUTPUT_DIR${NC}"
        fi
    fi
}

# 스크립트 시작
parse_arguments "$@"
main