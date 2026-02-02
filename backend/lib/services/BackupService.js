const BaseService = require('./BaseService');
const RepositoryFactory = require('../database/repositories/RepositoryFactory');
const fs = require('fs').promises;
const path = require('path');
const ConfigManager = require('../config/ConfigManager');

/**
 * BackupService class
 * Handles business logic for system backups and restores.
 */
class BackupService extends BaseService {
    constructor() {
        super(null);
        this.configManager = ConfigManager.getInstance();
        // __dirname: /app/backend/lib/services
        this.backupDir = path.resolve(__dirname, '../../../data/backup');

        // 자동 백업 스케줄러 시작
        this.startAutoBackupScheduler();
    }

    get settingsRepository() {
        if (!this._settingsRepo) {
            this._settingsRepo = RepositoryFactory.getInstance().getSystemSettingsRepository();
        }
        return this._settingsRepo;
    }

    get repository() {
        if (!this._repository) {
            this._repository = RepositoryFactory.getInstance().getBackupRepository();
        }
        return this._repository;
    }

    /**
     * 모든 백업 기록 조회 및 실제 파일 존재 여부 확인
     */
    async getBackups(options = {}) {
        return await this.handleRequest(async () => {
            const result = await this.repository.findAll(null, options);

            // 실제 파일 존재 여부 체크 가미
            const itemsWithFileStatus = await Promise.all(result.items.map(async (item) => {
                const fullPath = path.join(this.backupDir, item.filename);
                let exists = false;
                try {
                    await fs.access(fullPath);
                    exists = true;
                } catch (e) {
                    exists = false;
                }
                return { ...item, fileExists: exists };
            }));

            return { ...result, items: itemsWithFileStatus };
        }, 'GetBackups');
    }

    /**
     * 즉시 백업 수행
     */
    async createBackup(data, user = null) {
        return await this.handleRequest(async () => {
            const dbPath = this.configManager.get('SQLITE_DB_PATH', '../data/db/pulseone.db');
            const absoluteDbPath = path.isAbsolute(dbPath) ? dbPath : path.resolve(__dirname, '../../../', dbPath);

            const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
            const filename = `pulseone_backup_${timestamp}.db`;
            const fullBackupPath = path.join(this.backupDir, filename);

            // 1. 디렉토리 존재 확인
            await fs.mkdir(this.backupDir, { recursive: true });

            // 2. 파일 복사 (백업 실행)
            const startTime = Date.now();
            await fs.copyFile(absoluteDbPath, fullBackupPath);
            const duration = Math.floor((Date.now() - startTime) / 1000);

            // 3. 파일 정보 획득
            const stats = await fs.stat(fullBackupPath);

            // 4. DB 기록
            const backupId = await this.repository.create({
                name: data.name || `백업_${new Date().toLocaleDateString()}`,
                filename: filename,
                type: 'full',
                status: 'completed',
                size: stats.size,
                location: this.backupDir,
                created_by: user ? user.username : 'system',
                description: data.description || '',
                duration: duration
            });

            return { id: backupId, filename, size: stats.size };
        }, 'CreateBackup');
    }

    /**
     * 백업 삭제
     */
    async deleteBackup(id) {
        return await this.handleRequest(async () => {
            const backup = await this.repository.findById(id);
            if (!backup) throw new Error('백업을 찾을 수 없습니다.');

            const fullPath = path.join(this.backupDir, backup.filename);

            // 1. 실제 파일 삭제 시도
            try {
                await fs.unlink(fullPath);
            } catch (e) {
                console.warn(`⚠️ 파일 삭제 실패 (이미 없을 수 있음): ${fullPath}`);
            }

            // 2. DB 기록 Soft Delete
            await this.repository.softDelete(id);

            return { id, success: true };
        }, 'DeleteBackup');
    }

    /**
     * 백업에서 복원
     */
    async restoreBackup(id) {
        return await this.handleRequest(async () => {
            const backup = await this.repository.findById(id);
            if (!backup) throw new Error('백업을 찾을 수 없습니다.');

            const fullBackupPath = path.join(this.backupDir, backup.filename);
            const dbPath = this.configManager.get('SQLITE_DB_PATH', '../data/db/pulseone.db');
            const absoluteDbPath = path.isAbsolute(dbPath) ? dbPath : path.resolve(__dirname, '../../../', dbPath);

            // 1. 파일 존재 확인
            try {
                await fs.access(fullBackupPath);
            } catch (e) {
                throw new Error('백업 파일이 존재하지 않습니다.');
            }

            // 2. 현재 DB 백업 (안전을 위해)
            const safetyBackupPath = absoluteDbPath + '.safety_tmp';
            await fs.copyFile(absoluteDbPath, safetyBackupPath);

            try {
                // 3. 백업 파일로 덮어쓰기
                // 주의: 실시간 서비스 중에는 파일 잠금 문제가 있을 수 있음
                await fs.copyFile(fullBackupPath, absoluteDbPath);

                // 4. 임시 세이프티 파일 삭제
                await fs.unlink(safetyBackupPath);

                return {
                    success: true,
                    message: '복원이 완료되었습니다. 시스템 안정성을 위해 재시작을 권장합니다.'
                };
            } catch (restoreError) {
                // 실패 시 롤백
                await fs.copyFile(safetyBackupPath, absoluteDbPath);
                await fs.unlink(safetyBackupPath);
                throw restoreError;
            }
        }, 'RestoreBackup');
    }

    /**
     * 백업 설정 조회
     */
    async getSettings() {
        return await this.handleRequest(async () => {
            const settings = await this.settingsRepository.findByCategory('backup');

            // 기본값 설정 (없을 경우)
            const defaults = {
                'backup.auto_enabled': 'false',
                'backup.schedule_time': '02:00',
                'backup.retention_days': '30',
                'backup.include_logs': 'true'
            };

            const result = {};
            // DB에 있는 설정으로 매핑
            settings.forEach(s => {
                result[s.key_name] = s.value;
            });

            // 누락된 설정을 기본값으로 채움 (응답용)
            Object.keys(defaults).forEach(key => {
                if (result[key] === undefined) {
                    result[key] = defaults[key];
                }
            });

            return result;
        }, 'GetBackupSettings');
    }

    /**
     * 백업 설정 업데이트
     */
    async updateSettings(data, user = null) {
        return await this.handleRequest(async () => {
            const userId = user ? user.id : null;
            const promises = Object.entries(data).map(([key, value]) => {
                if (key.startsWith('backup.')) {
                    return this.settingsRepository.updateValue(key, value, userId);
                }
                return null;
            }).filter(p => p !== null);

            await Promise.all(promises);
            return { success: true };
        }, 'UpdateBackupSettings');
    }

    /**
     * 자동 백업 스케줄러
     * 1분 간격으로 설정을 확인하여 실행 여부 판단
     */
    startAutoBackupScheduler() {
        console.log('🕒 Backup Scheduler started');

        // 1분마다 체크 (IntervalID 보관 가능하나 싱글턴이므로 생략)
        setInterval(async () => {
            try {
                const settings = await this.getSettings();

                // 1. 자동 백업 활성화 여부 확인
                if (settings['backup.auto_enabled'] !== 'true') return;

                // 2. 예약 시간 확인 (HH:mm)
                const now = new Date();
                const currentUTCTime = now.toISOString().substring(11, 16); // "HH:mm"

                // 로컬 시간(KST) 기준 체크 (이 시스템은 기본적으로 KST 기준임을 가정하거나 Config 확인 필요)
                // KST = UTC + 9
                const kstOffset = 9 * 60 * 60 * 1000;
                const kstNow = new Date(now.getTime() + kstOffset);
                const currentTime = kstNow.toISOString().substring(11, 16);

                if (currentTime === settings['backup.schedule_time']) {
                    console.log(`🚀 [SCHEDULER] Scheduled backup triggered at ${currentTime}`);
                    await this.createBackup({
                        name: `자동 백업 (${currentTime})`,
                        description: '시스템 스케줄러에 의한 자동 백업'
                    });

                    // 백업 후 리텐션(정리) 도 수행
                    await this.performAutoRetention(parseInt(settings['backup.retention_days'] || '30'));
                }
            } catch (error) {
                console.error('❌ [SCHEDULER] Backup check failed:', error.message);
            }
        }, 60 * 1000);
    }

    /**
     * 보관 기간이 지난 백업 자동 삭제
     */
    async performAutoRetention(days) {
        if (!days || days <= 0) return;

        try {
            const backups = await this.repository.findAll();
            const retentionDate = new Date();
            retentionDate.setDate(retentionDate.getDate() - days);

            console.log(`🧹 [RETENTION] Cleaning up backups older than ${days} days (before ${retentionDate.toISOString()})`);

            for (const backup of backups.items) {
                const createdAt = new Date(backup.created_at);
                if (createdAt < retentionDate && backup.is_deleted === 0) {
                    console.log(`🗑️ [RETENTION] Deleting old backup: ${backup.filename}`);
                    await this.deleteBackup(backup.id);
                }
            }
        } catch (error) {
            console.error('❌ [RETENTION] Auto cleanup failed:', error.message);
        }
    }
}

module.exports = new BackupService();
