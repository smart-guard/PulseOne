/**
 * v49 - 가장 안전한 통합 UI 텍스트 & 에러 메시지 직접 치환
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
    ['프로파일 탭에서 설정한 데이터의 최종 이름', 'Final name of the data set in Profile tab'],
    ['데터 값 (VALUE)', 'Data Value (VALUE)'],
    ['데이터 값 (VALUE)', 'Data Value (VALUE)'],
    ['Scale/Offset 반영 실제 측정값', 'Actual measurement with Scale/Offset'],
    ['타켓 Description (DESC)', 'Target Description (DESC)'],
    ['타겟 설명 (DESC)', 'Target Description (DESC)'],
    ['프로파일에서 Settings한 데터 Description', 'Data description set in Profile'],
    ['프로파일에서 설정한 데이터 설명', 'Data description set in Profile'],
    ['수집기 원본 (Collector)', 'Collector Source'],
    ['건물 ID', 'Site ID'],
    ['원본 건물(Building) ID', 'Source Site ID'],
    ['일반적으로 1', 'Usually 1'],
    ['Point ID', 'Point ID'],
    ['원본 Point(Point) ID', 'Source Point ID'],
    ['원본 데이터포인트(Point) ID', 'Source data point ID'],
    ['원본 Name', 'Original Name'],
    ['원본 이름', 'Original Name'],
    ['수집기 내부 원본 Point명', 'Internal original point name of collector'],
    ['수집기 내부 원본 포인트명', 'Internal original point name of collector'],
    ['데터 타입', 'Data Type'],
    ['데이터 타입', 'Data Type'],
    ['num:숫자, bit:디지털, str:문자열', 'num:number, bit:digital, str:string'],
    ['Status 및 Alarm (Status)', 'Status & Alarm (Status)'],
    ['상태 및 알람 (Status)', 'Status & Alarm (Status)'],
    ['게이트웨이 삭제', 'Delete Gateway'],
    ['게트웨 Delete', 'Delete Gateway'],
    ['게트웨 정말 delete??', 'Really delete gateway?'],
    ['게이트웨이를 정말 삭제하시겠습니까?', 'Really delete gateway?'],
    ['작업 되돌릴 수 없습니다.', 'This action cannot be undone.'],
    ['이 작업은 되돌릴 수 없습니다.', 'This action cannot be undone.'],
    ['삭제 중 오류가 발생했습니다.', 'Error occurred during deletion.'],
    ['Delete 중 Error occurred.', 'Error occurred during deletion.'],
    ['게트웨 Start Confirm', 'Confirm Gateway Start'],
    ['게이트웨이 시작 확인', 'Confirm Gateway Start'],
    ['게트웨 프로세스 start??', 'start gateway process?'],
    ['게이트웨이 프로세스를 시작하시겠습니까?', 'start gateway process?'],
    ['게이트웨이 프로세스가 성공적으로 시작되었습니다.', 'Gateway process started successfully.'],
    ['게트웨 프로세스 Success적으로 Start되었습니다.', 'Gateway process started successfully.'],
    ['프로세스 Start에 failed.', 'Failed to start process.'],
    ['프로세스 시작에 실패했습니다.', 'Failed to start process.'],
    ['API 호출 중 Error occurred.', 'Error occurred during API call.'],
    ['API 호출 중 오류가 발생했습니다.', 'Error occurred during API call.'],
    ['게트웨 Stop Confirm', 'Confirm Gateway Stop'],
    ['게이트웨이 중지 확인', 'Confirm Gateway Stop'],
    ['게트웨 프로세스 stop??', 'stop gateway process?'],

    // Script Engine
    ['절댓값', 'Absolute Value'],
    ['숫자의 절댓값을 반환합니다.', 'Returns absolute value of number.'],
    ['입력 숫자', 'Input Number'],
    ['기본 사용법', 'Default Usage'],
    ['음수의 절댓값', 'Absolute value of negative'],
    ['양수 입력', 'Positive Input'],
    ['양수의 절댓값', 'Absolute value of positive'],
    ['제곱근', 'Square Root'],
    ['숫자의 제곱근을 반환합니다.', 'Returns square root of number.'],
    ['입력 숫자 (0 이상)', 'Input Number (0 or more)'],

    // Error/Logs in UI
    ['Gateway 이름은 필수입니다.', 'Gateway Name is required.'],
    ['Host 필드는 필수입니다.', 'Host field is required.'],
    ['Topic은 설정되었으나 페이로드 템플릿이 선택되지 않았습니다.', 'Topic is set but payload template is not selected.'],
    ['타겟 키 매핑이 올바르지 않습니다.', 'Target key mapping is invalid.'],
    ['Gateway를 먼저 생성/저장한 뒤 수동 테스트를 진행하세요.', 'Please Save Gateway first to run a manual test.'],
    ['Gateway 저장을 먼저 진행해주세요.', 'Please Save Gateway first.'],
    ['입력 데이터를 JSON으로 파싱할 수 없습니다.', 'Cannot parse input data as JSON.'],
    ['테스트 성공!', 'Test Successful!'],
    ['ExportGateway가 성공적으로 생성되었습니다.', 'Export Gateway created successfully.'],
    ['ExportGateway 정보가 업데이트되었습니다.', 'Export Gateway updated successfully.'],
    ['Target Key Mapping 저장 중 오류가 발생했습니다.', 'Error occurred while saving target key mapping.'],
    ['데이터를 파싱할 수 없거나 형식이 잘못되었습니다.', 'Cannot parse data or invalid format.'],
    ['적어도 1개 이상의 유효한 매핑 데이터가 필요합니다.', 'At least 1 valid mapping data is required.'],
    ['에러 내용:', 'Error details:'],
    ['Target Key Mapping을 포함한 모든 설정이 삭제됩니다.', 'All settings including Target Key Mapping will be deleted.'],
    ['Gateway 삭제 완료', 'Gateway deleted successfully'],
    ['선택된 Gateway가 없습니다.', 'No Gateway selected.'],
    ['수동 테스트', 'Manual Test'],
    ['테스트 실행', 'Run Test'],
    ['게이트웨이 선택', 'Select Gateway'],
    ['요청', 'Request'],
    ['응답', 'Response'],
    ['실행 중...', 'Running...'],

    // Protocols Dashboard common text
    ['지원 데이터를 대시보드 형식으로 변환', 'Convert supported data to dashboard format'],
    ['MQTT Broker 서비스', 'MQTT Broker Service'],
    ['Operations 및 Security Settings', 'Operations and Security Settings'],

    // Fallbacks for words with exact boundaries if possible
    ['수정', 'Edit'],
    ['삭제', 'Delete'],
    ['성공적으로', 'successfully'],
    ['실패했습니다', 'failed'],
    ['오류가 발생했습니다', 'error occurred'],
    ['진행하시겠습니까', 'proceed?'],
    ['취소', 'Cancel'],
    ['확인', 'Confirm']
];

for (const fp of allFiles) {
    let content = fs.readFileSync(fp, 'utf-8');
    if (!koreanRe.test(content)) continue;

    let originalContent = content;
    let fileReplacements = 0;

    const lines = content.split('\n');
    for (let i = 0; i < lines.length; i++) {
        let line = lines[i];

        // // 주석 부분 한글 안전하게 영어 처리
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

        // 멀티라인 블록 주석의 단일라인 처리
        const blockIdx = line.indexOf('/*');
        if (blockIdx !== -1 && line.indexOf('*/', blockIdx) !== -1) {
            const blockContent = line.substring(blockIdx, line.indexOf('*/') + 2);
            if (koreanRe.test(blockContent)) {
                lines[i] = line.replace(blockContent, '/* [translated comment] */');
                fileReplacements++;
                line = lines[i];
            }
        }

        if (koreanRe.test(lines[i])) {
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
