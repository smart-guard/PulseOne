// backend/lib/services/ConfigEditorService.js
const fs = require('fs');
const path = require('path');
const LogManager = require('../utils/LogManager');

class ConfigEditorService {
    constructor() {
        this.logger = LogManager.getInstance();
        // Use process.cwd() (backend root) to find config sibling directory
        // In Docker: /app/backend -> ../config -> /app/config
        this.configDir = path.resolve(process.cwd(), '../config');

        // 허용된 확장자
        this.allowedExtensions = ['.env', '.json', '.conf'];
    }

    /**
     * 설정 파일 목록 조회
     */
    async listFiles() {
        try {
            this.logger.debug('ConfigEditor', `Listing files from: ${this.configDir}`);

            if (!fs.existsSync(this.configDir)) {
                this.logger.warn('ConfigEditor', `Config directory not found: ${this.configDir}`);
                return { files: [], path: this.configDir };
            }

            const files = await fs.promises.readdir(this.configDir);

            const configFiles = [];
            for (const file of files) {
                // 확장자 필터링
                const ext = path.extname(file);
                // .env로 시작하거나(숨김파일) 허용된 확장자인 경우
                const isEnv = file.startsWith('.env') || file.endsWith('.env');
                const isAllowed = this.allowedExtensions.includes(ext) || isEnv;

                if (isAllowed) {
                    const stats = await fs.promises.stat(path.join(this.configDir, file));
                    if (stats.isFile()) {
                        configFiles.push({
                            name: file,
                            size: stats.size,
                            updatedAt: stats.mtime
                        });
                    }
                }
            }

            return {
                files: configFiles.sort((a, b) => a.name.localeCompare(b.name)),
                path: this.configDir
            };
        } catch (error) {
            this.logger.error('ConfigEditor', `List files failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * 파일 내용 조회
     */
    async getFileContent(filename) {
        try {
            const filePath = this._resolveSafePath(filename);
            const content = await fs.promises.readFile(filePath, 'utf8');
            return content;
        } catch (error) {
            this.logger.error('ConfigEditor', `Get content failed for ${filename}: ${error.message}`);
            throw error;
        }
    }

    /**
     * 파일 내용 저장
     */
    async saveFileContent(filename, content) {
        try {
            const filePath = this._resolveSafePath(filename);

            // 백업 생성 (선택사항, 안전을 위해 권장)
            const backupPath = `${filePath}.bak`;
            if (fs.existsSync(filePath)) {
                await fs.promises.copyFile(filePath, backupPath);
            }

            await fs.promises.writeFile(filePath, content, 'utf8');

            this.logger.system('INFO', `Config file updated: ${filename}`);
            return true;
        } catch (error) {
            this.logger.error('ConfigEditor', `Save failed for ${filename}: ${error.message}`);
            throw error;
        }
    }

    /**
     * 경로 안전성 검증 (Path Traversal 방지)
     */
    _resolveSafePath(filename) {
        // 파일명에 경로 구분자가 있으면 거부 (단순 파일명만 허용)
        if (filename.includes('/') || filename.includes('\\')) {
            throw new Error('Invalid filename');
        }

        const safePath = path.join(this.configDir, filename);

        // 최종 경로가 configDir 내부인지 확인
        if (!safePath.startsWith(this.configDir)) {
            throw new Error('Access denied');
        }

        return safePath;
    }

    /**
     * 시크릿 암호화 (XOR + Base64)
     * C++ SecretManager와 동일한 로직/키 사용
     */
    encryptSecret(value) {
        if (!value) return '';

        const KEY = "PulseOne2025SecretKey";
        let encrypted = '';

        // XOR Encryption
        for (let i = 0; i < value.length; i++) {
            encrypted += String.fromCharCode(value.charCodeAt(i) ^ KEY.charCodeAt(i % KEY.length));
        }

        // Base64 Encoding
        const encoded = Buffer.from(encrypted, 'binary').toString('base64');
        return `ENC:${encoded}`;
    }

    /**
     * 시크릿 복호화
     */
    decryptSecret(value) {
        if (!value || !value.startsWith('ENC:')) return value;

        const KEY = "PulseOne2025SecretKey";
        const encryptedBase64 = value.substring(4); // Remove ENC:

        try {
            const encryptedStr = Buffer.from(encryptedBase64, 'base64').toString('binary');
            let decrypted = '';

            for (let i = 0; i < encryptedStr.length; i++) {
                decrypted += String.fromCharCode(encryptedStr.charCodeAt(i) ^ KEY.charCodeAt(i % KEY.length));
            }
            return decrypted;
        } catch (e) {
            this.logger.error('ConfigEditor', `Decryption failed: ${e.message}`);
            throw new Error('Invalid encrypted value');
        }
    }
}

module.exports = ConfigEditorService;
