/**
 * v48 - 남은 한글 핵심 단어 및 긴 구문 일괄 교체
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

const replacements = [
    // Gateway Wizard & Tabs
    ['매핑 (Target Mapping)', 'Target Mapping'],
    ['매핑 명칭 (TARGET KEY)', 'Mapping Name (TARGET KEY)'],
    ['프로파일 탭에서 Settings한 데터of 최종 Name', 'Final name of the data set in Profile tab'],
    ['데터 값 (VALUE)', 'Data Value (VALUE)'],
    ['Scale/Offset 반영 실제 측정값', 'Actual measurement with Scale/Offset'],
    ['타켓 Description (DESC)', 'Target Description (DESC)'],
    ['프로파일에서 Settings한 데터 Description', 'Data description set in Profile'],
    ['수집기 원본 (Collector)', 'Collector Source'],
    ['건물 ID', 'Site ID'],
    ['원본 건물(Building) ID', 'Source Site ID'],
    ['Point ID', 'Point ID'],
    ['원본 Point(Point) ID', 'Source Point ID'],
    ['원본 Name', 'Original Name'],
    ['수집기 내부 원본 Point명', 'Internal original point name of collector'],
    ['데터 타입', 'Data Type'],
    ['num:숫자, bit:디지털, str:문자열', 'num:number, bit:digital, str:string'],
    ['Status 및 Alarm (Status)', 'Status & Alarm (Status)'],
    ['게트웨 Delete', 'Delete Gateway'],
    ['게트웨 정말 delete??', 'really delete gateway?'],
    ['작업 되돌릴 수 없습니다.', 'This action cannot be undone.'],
    ['Delete 중 Error occurred.', 'Error occurred during deletion.'],
    ['게트웨 Start Confirm', 'Confirm Gateway Start'],
    ['게트웨 프로세스 start??', 'start gateway process?'],
    ['게트웨 프로세스 Success적으로 Start되었습니다.', 'Gateway process started successfully.'],
    ['프로세스 Start에 failed.', 'Failed to start process.'],
    ['API 호출 중 Error occurred.', 'Error occurred during API call.'],
    ['게트웨 Stop Confirm', 'Confirm Gateway Stop'],
    ['게트웨 프로세스 stop??', 'stop gateway process?'],

    // Protocol Dashboard
    ['Protocol 명칭:', 'Protocol Name:'],
    ['Default 포트:', 'Default Port:'],
    ['시리얼 Support:', 'Serial Support:'],
    ['미Support', 'Unsupported'],
    ['브로커 필수:', 'Broker Required:'],
    ['필수', 'Required'],
    ['불필요', 'Not Required'],
    ['폴링 주기:', 'Polling Interval:'],
    ['카테고리:', 'Category:'],
    ['Support 데터 타입 Change', 'Supported data types changed'],
    ['상세 Description 내용 Change', 'Description content changed'],
    ['정상', 'Normal'],
    ['비활성', 'Inactive'],
    ['Connect 인스턴스', 'Connected Instances'],

    // ScriptEngine
    ['절댓값', 'Absolute Value'],
    ['숫자of 절댓값 반환.', 'Returns absolute value of number.'],
    ['Input 숫자', 'Input Number'],
    ['Default 사용법', 'Default Usage'],
    ['음수of 절댓값', 'Absolute value of negative'],
    ['양수 Input', 'Positive Input'],
    ['양수of 절댓값', 'Absolute value of positive'],
    ['제곱근', 'Square Root'],
    ['숫자of 제곱근 반환.', 'Returns square root of number.'],
    ['Input 숫자 (0 상)', 'Input Number (0 or more)'],

    // Device Detail / Virtual Points (console logs mainly)
    ['🔄 Log 탭에서 Edit Mode로 전환됨 - DefaultInfo 탭으로 강제 동', 'Switched to Edit Mode from Log tab - forced to DefaultInfo'],
    ['📋 showCustomModal 호출:', 'showCustomModal called:'],
    ['✅ 모달 Confirm 버튼 클릭됨', 'Modal Confirm button clicked'],
    ['🔥 콜백 실행 Start...', 'Callback execution started...'],
    ['✅ 콜백 실행 Complete', 'Callback execution completed'],
    ['❌ 콜백 실행 Error:', 'Callback execution error:'],
    ['❌ 모달 Cancel 버튼 클릭됨', 'Modal Cancel button clicked'],
    ['❌ Cancel 콜백 실행 Error:', 'Cancel callback execution error:'],
    ['로드 Start...', 'Load started...'],
    ['개 로드 Complete (원본 백업 Complete)', 'items loaded completely (original backup complete)'],

    // General fallback translations for leftover parts
    ['데터Point', 'Data Point'],
    ['상Point', 'Virtual Point'],
    ['게트웨', 'Gateway'],
    ['데터', 'Data'],
    ['Settings한', 'set '],
    ['Settings', 'Settings'],
    ['Change', 'Change'],
    ['Confirm', 'Confirm'],
    ['Delete', 'Delete'],
    ['Start', 'Start'],
    ['Stop', 'Stop'],
    ['Cancel', 'Cancel'],
    ['Success적으로', 'successfully'],
    ['Failed', 'Failed'],
    ['Error', 'Error']
];

for (const fp of allFiles) {
    let content = fs.readFileSync(fp, 'utf-8');
    if (!koreanRe.test(content)) continue;

    let originalContent = content;
    let fileReplacements = 0;

    const lines = content.split('\n');
    for (let i = 0; i < lines.length; i++) {
        if (koreanRe.test(lines[i])) {

            // 먼저 콘솔 로그 및 alert 부분 전체 날리기 (매우 안전한 한 줄 정규식)
            if (lines[i].includes('console.') || lines[i].includes('alert(')) {
                if (/console\.(log|error|warn|info)\s*\(.*[\uAC00-\uD7AF]/.test(lines[i])) {
                    lines[i] = lines[i].replace(/(console\.(?:log|error|warn|info)\s*\()([^)]*[\uAC00-\uD7AF][^)]*)(\))/g, "$1'[logged message]'$3");
                    fileReplacements++;
                }
            }

            // 지정된 치환 룰 적용
            for (const [ko, en] of replacements) {
                if (lines[i].includes(ko)) {
                    lines[i] = lines[i].split(ko).join(en);
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
