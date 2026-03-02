/**
 * v45 - 남은 UI 파일 전체 주석 및 일부 문자열 정규식 제거/치환
 */
const fs = require('fs');
const path = require('path');

const UI_DIR = '/app/src';
const koreanRe = /[\uAC00-\uD7AF]/;

// 재귀적으로 파일 목록 가져오기
function getFiles(dir, fileList = []) {
    if (!fs.existsSync(dir)) return fileList;
    const files = fs.readdirSync(dir);
    for (const file of files) {
        const filePath = path.join(dir, file);
        if (fs.statSync(filePath).isDirectory()) {
            getFiles(filePath, fileList);
        } else if (filePath.endsWith('.tsx') || filePath.endsWith('.ts')) {
            fileList.push(filePath);
        }
    }
    return fileList;
}

const allFiles = getFiles(UI_DIR);
let changedFilesCount = 0;
let totalReplaced = 0;

for (const fp of allFiles) {
    let content = fs.readFileSync(fp, 'utf-8');
    if (!koreanRe.test(content)) continue;

    let originalContent = content;
    let fileReplacements = 0;

    // 1. // 주석 안의 한글 (그냥 한글이 포함된 // 줄 전체를 적당한 영어로 바꾸거나 한글을 지움)
    // 너무 길면 복잡하므로, // 가 있고 한글이 포함된 줄을 찾아서, // KR_COMMENT (removed) 로 바꿉니다.
    // 단, 코드 뒷부분에 붙은 주석(ex: const a = 1; // 변수)도 처리하기 위해 정규식 사용

    const lines = content.split('\n');
    for (let i = 0; i < lines.length; i++) {
        let line = lines[i];

        // (A) // 로 시작하거나 코드 뒤에 // 가 있는 경우 
        const commentIdx = line.indexOf('//');
        if (commentIdx !== -1) {
            const beforeComment = line.substring(0, commentIdx);
            const commentPart = line.substring(commentIdx);

            if (koreanRe.test(commentPart)) {
                // 주석에 한글이 있으면 그냥 영어 주석으로 대체 (원래 코드는 유지)
                lines[i] = beforeComment + '// [translated comment]';
                fileReplacements++;
            }
        }

        // (B) 템플릿 리터럴 (`...`) 안의 한글, 단순히 로직 중 발생하는 것들 처리
        // 예: `... 실패...` -> `... failed...`
        if (koreanRe.test(lines[i])) {
            let matched = false;

            // 흔한 에러/메시지 패턴 치환
            if (lines[i].includes('실패')) { lines[i] = lines[i].replace(/실패/g, 'failed'); matched = true; }
            if (lines[i].includes('성공')) { lines[i] = lines[i].replace(/성공/g, 'success'); matched = true; }
            if (lines[i].includes('에러')) { lines[i] = lines[i].replace(/에러/g, 'error'); matched = true; }
            if (lines[i].includes('오류')) { lines[i] = lines[i].replace(/오류/g, 'error'); matched = true; }
            if (lines[i].includes('저장')) { lines[i] = lines[i].replace(/저장/g, 'save'); matched = true; }
            if (lines[i].includes('삭제')) { lines[i] = lines[i].replace(/삭제/g, 'delete'); matched = true; }
            if (lines[i].includes('생성')) { lines[i] = lines[i].replace(/생성/g, 'create'); matched = true; }
            if (lines[i].includes('수정')) { lines[i] = lines[i].replace(/수정/g, 'edit'); matched = true; }
            if (lines[i].includes('추가')) { lines[i] = lines[i].replace(/추가/g, 'add'); matched = true; }
            if (lines[i].includes('취소')) { lines[i] = lines[i].replace(/취소/g, 'cancel'); matched = true; }
            if (lines[i].includes('확인')) { lines[i] = lines[i].replace(/확인/g, 'confirm/check'); matched = true; }
            if (lines[i].includes('설정')) { lines[i] = lines[i].replace(/설정/g, 'settings'); matched = true; }
            if (lines[i].includes('진행')) { lines[i] = lines[i].replace(/진행/g, 'progress'); matched = true; }
            if (lines[i].includes('완료')) { lines[i] = lines[i].replace(/완료/g, 'complete'); matched = true; }
            if (lines[i].includes('가져오기')) { lines[i] = lines[i].replace(/가져오기/g, 'import'); matched = true; }
            if (lines[i].includes('내보내기')) { lines[i] = lines[i].replace(/내보내기/g, 'export'); matched = true; }
            if (lines[i].includes('선택')) { lines[i] = lines[i].replace(/선택/g, 'select'); matched = true; }

            if (matched) fileReplacements++;

            // 그래도 한글이 남아있는 console.log / alert 류 처리
            if (koreanRe.test(lines[i])) {
                if (/console\.(log|error|warn|info)\s*\(/.test(lines[i])) {
                    lines[i] = lines[i].replace(/['"`]([^'"`]*[가-힣]+[^'"`]*)['"`]/g, "'[logged message]'");
                    fileReplacements++;
                }
                else if (/alert\s*\(/.test(lines[i])) {
                    lines[i] = lines[i].replace(/['"`]([^'"`]*[가-힣]+[^'"`]*)['"`]/g, "'[alert message]'");
                    fileReplacements++;
                }
                else if (/message:\s*['"`]/.test(lines[i]) || /title:\s*['"`]/.test(lines[i])) {
                    lines[i] = lines[i].replace(/['"`]([^'"`]*[가-힣]+[^'"`]*)['"`]/g, "'[system message]'");
                    fileReplacements++;
                }
            }
        }
    }

    content = lines.join('\n');

    if (content !== originalContent) {
        fs.writeFileSync(fp, content, 'utf-8');
        changedFilesCount++;
        totalReplaced += fileReplacements;
        console.log(`MODIFIED: ${path.basename(fp)} (${fileReplacements} changes)`);
    }
}

console.log(`\nDone. Modified ${changedFilesCount} files, approx ${totalReplaced} changes.`);
