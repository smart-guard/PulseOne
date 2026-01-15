const express = require('express');
const router = express.Router();
const { authenticateToken, requireRole } = require('../middleware/tenantIsolation');
const { createResponse } = require('../lib/services/BaseService');

const RoleRepository = require('../lib/database/repositories/RoleRepository');
const PermissionRepository = require('../lib/database/repositories/PermissionRepository');
const RoleService = require('../lib/services/RoleService');

// Instantiate without DB arguments
const roleRepo = new RoleRepository();
const permRepo = new PermissionRepository();
const roleService = new RoleService(roleRepo, permRepo); // BaseService keys off first arg as this.repository

// Helper for async errors
const asyncHandler = fn => (req, res, next) =>
    Promise.resolve(fn(req, res, next)).catch(next);

// GET /api/system/permissions
router.get('/permissions', authenticateToken, asyncHandler(async (req, res) => {
    const permissions = await roleService.getAllPermissions();
    res.json({ success: true, data: permissions }); // Simplistic response format matching BaseService or manual
}));

// GET /api/system/roles
router.get('/roles', authenticateToken, asyncHandler(async (req, res) => {
    const roles = await roleService.getAllRoles();
    res.json({ success: true, data: roles });
}));

// GET /api/system/roles/:id
router.get('/roles/:id', authenticateToken, asyncHandler(async (req, res) => {
    const role = await roleService.getRole(req.params.id);
    if (!role) {
        return res.status(404).json({ success: false, error: 'Role not found' });
    }
    res.json({ success: true, data: role });
}));

// POST /api/system/roles
router.post('/roles', authenticateToken, requireRole('system_admin'), asyncHandler(async (req, res) => {
    const { name, description, permissions } = req.body;
    const newRole = await roleService.createRole({ name, description }, permissions);
    res.json({ success: true, data: newRole });
}));

// PUT /api/system/roles/:id
router.put('/roles/:id', authenticateToken, requireRole('system_admin'), asyncHandler(async (req, res) => {
    const { name, description, permissions } = req.body;
    const updatedRole = await roleService.updateRole(req.params.id, { name, description }, permissions);
    res.json({ success: true, data: updatedRole });
}));

// DELETE /api/system/roles/:id
router.delete('/roles/:id', authenticateToken, requireRole('system_admin'), asyncHandler(async (req, res) => {
    await roleService.deleteRole(req.params.id);
    res.json({ success: true, message: 'Role deleted successfully' });
}));

module.exports = router;
