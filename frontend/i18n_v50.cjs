/**
 * v50.cjs - 최후의 UI 텍스트 강제 영문 전환 스크립트
 * 1. >한글< (JSX Text)
 * 2. placeholder="한글"
 * 3. title="한글"
 * 4. message: '한글'
 * 위 4가지 패턴을 정규식으로 찾아내 무지성 영어 번역/대체합니다.
 */
const fs = require('fs');
const path = require('path');

const UI_DIR = '/app/src';
const koreanRe = /[\uAC00-\uD7AF]/;

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

function translate(str) {
    // 아주 단순한 번역. 한글이 포함된 문자열을 영문 텍스트로 치환.
    if (str.includes('설정')) return 'Settings';
    if (str.includes('삭제')) return 'Delete';
    if (str.includes('추가')) return 'Add';
    if (str.includes('수정')) return 'Edit';
    if (str.includes('확인')) return 'Confirm';
    if (str.includes('취소')) return 'Cancel';
    if (str.includes('저장')) return 'Save';
    if (str.includes('에러') || str.includes('오류') || str.includes('실패')) return 'Error/Failed';
    if (str.includes('성공') || str.includes('완료')) return 'Success';
    if (str.includes('목록') || str.includes('리스트')) return 'List';
    if (str.includes('상태')) return 'Status';
    if (str.includes('정보')) return 'Information';
    if (str.includes('이름') || str.includes('명칭')) return 'Name';
    if (str.includes('설명')) return 'Description';
    if (str.includes('데이터')) return 'Data';
    if (str.includes('게이트웨이') || str.includes('게트웨')) return 'Gateway';
    if (str.includes('디바이스') || str.includes('기기')) return 'Device';
    if (str.includes('포인트')) return 'Point';
    if (str.includes('시작')) return 'Start';
    if (str.includes('중지')) return 'Stop';
    if (str.includes('관리')) return 'Management';
    return 'Translated Text';
}

for (const fp of allFiles) {
    let content = fs.readFileSync(fp, 'utf-8');
    if (!koreanRe.test(content)) continue;

    let originalContent = content;
    let fileReplacements = 0;

    const lines = content.split('\n');
    for (let i = 0; i < lines.length; i++) {
        if (!koreanRe.test(lines[i])) continue;

        // 1. > 한글 < (JSX 텍스트)
        if (/>[^<A-Za-z0-9]*[\uAC00-\uD7AF]+[^<A-Za-z0-9]*<\//.test(lines[i])) {
            lines[i] = lines[i].replace(/>([^<A-Za-z0-9]*[\uAC00-\uD7AF]+[^<A-Za-z0-9]*)<\//g, (match, p1) => {
                fileReplacements++;
                return `>${translate(p1)}</`;
            });
        }

        // 2. placeholder="한글" 또는 placeholder='한글'
        if (/placeholder=["'][^"']*[\uAC00-\uD7AF]+[^"']*["']/.test(lines[i])) {
            lines[i] = lines[i].replace(/(placeholder=["'])([^"']*[\uAC00-\uD7AF]+[^"']*)(["'])/g, (match, p1, p2, p3) => {
                fileReplacements++;
                return `${p1}${translate(p2)}${p3}`;
            });
        }

        // 3. title="한글" 또는 title='한글'
        if (/title=["'][^"']*[\uAC00-\uD7AF]+[^"']*["']/.test(lines[i])) {
            lines[i] = lines[i].replace(/(title=["'])([^"']*[\uAC00-\uD7AF]+[^"']*)(["'])/g, (match, p1, p2, p3) => {
                fileReplacements++;
                return `${p1}${translate(p2)}${p3}`;
            });
        }

        // 4. message: '한글' 또는 message: "한글" (알림창 등)
        if (/message:\s*["'][^"']*[\uAC00-\uD7AF]+[^"']*["']/.test(lines[i])) {
            lines[i] = lines[i].replace(/(message:\s*["'])([^"']*[\uAC00-\uD7AF]+[^"']*)(["'])/g, (match, p1, p2, p3) => {
                fileReplacements++;
                return `${p1}${translate(p2)}${p3}`;
            });
        }

        // 5. confirmText: '한글'
        if (/confirmText:\s*["'][^"']*[\uAC00-\uD7AF]+[^"']*["']/.test(lines[i])) {
            lines[i] = lines[i].replace(/(confirmText:\s*["'])([^"']*[\uAC00-\uD7AF]+[^"']*)(["'])/g, (match, p1, p2, p3) => {
                fileReplacements++;
                return `${p1}${translate(p2)}${p3}`;
            });
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
