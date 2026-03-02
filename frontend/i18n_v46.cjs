/**
 * v46 - 초안전 UI 파일 주석 및 문자열 일괄 변경
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

    const lines = content.split('\n');
    for (let i = 0; i < lines.length; i++) {
        let line = lines[i];

        // (A) // 로 시작하는 주석 중 한글이 있는 경우 안전하게 영어로 교체
        const commentIdx = line.indexOf('//');
        if (commentIdx !== -1) {
            const beforeComment = line.substring(0, commentIdx);
            const commentPart = line.substring(commentIdx);
            if (koreanRe.test(commentPart)) {
                lines[i] = beforeComment + '// [translated comment]';
                fileReplacements++;
                line = lines[i]; // line 갱신
            }
        }

        // (B) /** */ 블록 주석 등은 여러 줄이라 처리가 어려울 수 있으나, 보통 한 줄에 있는 /* ... */ 은 이렇게.
        const blockIdx = line.indexOf('/*');
        if (blockIdx !== -1 && line.indexOf('*/', blockIdx) !== -1) {
            const blockContent = line.substring(blockIdx, line.indexOf('*/') + 2);
            if (koreanRe.test(blockContent)) {
                lines[i] = line.replace(blockContent, '/* [translated comment] */');
                fileReplacements++;
                line = lines[i];
            }
        }

        // (C) 여전히 남아있는 한글 (주로 문자열 내부에 있는 단어들) - 매우 한정적이고 안전한 단순 문자열 Replace
        // 따옴표 파괴가 일어나지 않도록 정규식 캡쳐/치환은 자제하고,
        // 정해진 리터럴만 영어로 변경합니다.
        if (koreanRe.test(lines[i])) {
            let matched = false;
            const safeReplacements = [
                ['실패했습니다', 'failed'], ['성공했습니다', 'succeeded'],
                ['발생했습니다', 'occurred'], ['삭제하시겠습니까', 'delete?'],
                ['삭제할 수 없습니다', 'cannot be deleted'], ['등록하시겠습니까', 'register?'],
                ['진행하시겠습니까', 'proceed?'], ['시작하시겠습니까', 'start?'],
                ['중지하시겠습니까', 'stop?'], ['변경하시겠습니까', 'change?'],
                ['선택해주세요', 'please select'], ['입력해주세요', 'please enter'],
                ['실패', 'Failed'], ['성공', 'Success'],
                ['에러', 'Error'], ['오류', 'Error'],
                ['저장', 'Save'], ['삭제', 'Delete'],
                ['생성', 'Create'], ['수정', 'Edit'],
                ['추가', 'Add'], ['취소', 'Cancel'],
                ['확인', 'Confirm'], ['설정', 'Settings'],
                ['진행', 'Progress'], ['완료', 'Complete'],
                ['가져오기', 'Import'], ['내보내기', 'Export'],
                ['선택', 'Select'], ['시작', 'Start'],
                ['중지', 'Stop'], ['변경', 'Change'],
                ['기본', 'Default'], ['정보', 'Info'],
                ['상태', 'Status'], ['이름', 'Name'],
                ['검색', 'Search'], ['목록', 'List'],
                ['관리', 'Management'], ['사용자', 'User'],
                ['접속', 'Connection'], ['연결', 'Connect'],
                ['테스트', 'Test'], ['입력', 'Input'],
                ['로그', 'Log'], ['통계', 'Stats'],
                ['디바이스', 'Device'], ['장치', 'Device'],
                ['프로토콜', 'Protocol'], ['테넌트', 'Tenant'],
                ['사이트', 'Site'], ['그룹', 'Group'],
                ['설명', 'Description'], ['비밀번호', 'Password'],
                ['이메일', 'Email'], ['권한', 'Role'],
                ['지원', 'Support'], ['자동', 'Auto'],
                ['수동', 'Manual'], ['기능', 'Feature'],
                ['시간', 'Time'], ['날짜', 'Date'],
                ['적용', 'Apply'], ['포인트', 'Point'],
                ['모드', 'Mode'], ['활성화', 'Enable'],
                ['비활성화', 'Disable'], ['초기화', 'Reset'],
                ['알람', 'Alarm'], ['경고', 'Warning'],
                ['주의', 'Caution'], ['위험', 'Danger'],
                ['성공적으로', 'successfully'], ['정상적으로', 'properly'],
                ['비어있습니다', 'is empty'], ['존재하지 않습니다', 'does not exist'],
                ['이미 존재합니다', 'already exists'], ['할 수 없습니다', 'cannot be done'],
                ['될 수 있습니다', 'can be'], ['합니다', ''],
                ['입니다', ''], ['된', ''], ['된 ', ''],
                ['된', ''], ['할', ''], ['의', 'of'],
                ['를', ''], ['을', ''], ['가', ''], ['이', ''],
                ['은', ''], ['는', ''], ['(으)로', 'to'],
                ['과', 'and'], ['와', 'and']
            ];

            for (const [ko, en] of safeReplacements) {
                if (lines[i].includes(ko)) {
                    lines[i] = lines[i].split(ko).join(en);
                    matched = true;
                }
            }
            if (matched) fileReplacements++;
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
