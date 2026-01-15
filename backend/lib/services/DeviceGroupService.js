const BaseService = require('./BaseService');
const RepositoryFactory = require('../database/repositories/RepositoryFactory');

/**
 * DeviceGroupService class
 * Handles business logic for logical device groups.
 */
class DeviceGroupService extends BaseService {
    constructor() {
        super(null);
    }

    get repository() {
        if (!this._repository) {
            this._repository = RepositoryFactory.getInstance().getDeviceGroupRepository();
        }
        return this._repository;
    }

    /**
     * 모든 그룹 조회 (트리 구조로 변환 가능)
     */
    async getAllGroups(tenantId, options = {}) {
        return await this.handleRequest(async () => {
            const groups = await this.repository.findAll(tenantId, options.siteId);

            if (options.tree) {
                return this.buildGroupTree(groups);
            }

            return groups;
        }, 'GetAllGroups');
    }

    /**
     * 그룹 트리 빌더
     */
    buildGroupTree(groups) {
        const map = new Map();
        const roots = [];

        groups.forEach(group => {
            map.set(group.id, { ...group, children: [] });
        });

        groups.forEach(group => {
            if (group.parent_group_id && map.has(group.parent_group_id)) {
                map.get(group.parent_group_id).children.push(map.get(group.id));
            } else {
                roots.push(map.get(group.id));
            }
        });

        return roots;
    }

    /**
     * 그룹 상세 정보 조회
     */
    async getGroupDetail(id, tenantId) {
        return await this.handleRequest(async () => {
            const group = await this.repository.findById(id, tenantId);
            if (!group) throw new Error('Group not found');
            return group;
        }, 'GetGroupDetail');
    }

    /**
     * 신규 그룹 생성
     */
    async createGroup(groupData, tenantId) {
        return await this.handleRequest(async () => {
            return await this.repository.create(groupData, tenantId);
        }, 'CreateGroup');
    }

    /**
     * 그룹 정보 수정
     */
    async updateGroup(id, updateData, tenantId) {
        return await this.handleRequest(async () => {
            const updated = await this.repository.update(id, updateData, tenantId);
            if (!updated) throw new Error('Group not found or update failed');
            return updated;
        }, 'UpdateGroup');
    }

    /**
     * 그룹 삭제
     */
    async deleteGroup(id, tenantId) {
        return await this.handleRequest(async () => {
            const success = await this.repository.deleteById(id, tenantId);
            if (!success) throw new Error('Group not found or delete failed');
            return { id, success: true };
        }, 'DeleteGroup');
    }

    /**
     * 특정 그룹의 모든 하위 그룹 ID 목록을 조회 (본인 포함)
     */
    async getDescendantIds(groupId, tenantId) {
        return await this.handleRequest(async () => {
            const allGroups = await this.repository.findAll(tenantId);
            const descendantIds = [parseInt(groupId)];

            const findChildren = (parentId) => {
                const children = allGroups.filter(g => g.parent_group_id === parentId);
                children.forEach(child => {
                    descendantIds.push(child.id);
                    findChildren(child.id);
                });
            };

            findChildren(parseInt(groupId));
            return descendantIds;
        }, 'GetDescendantIds');
    }
}

module.exports = new DeviceGroupService();
