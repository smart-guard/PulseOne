-- 1. Restore Gateway Assignment to "insite 알람셋" (Profile ID 3)
DELETE FROM export_profile_assignments WHERE gateway_id = 5;
INSERT INTO export_profile_assignments (gateway_id, profile_id) VALUES (5, 3);

-- 2. Cleanup Garbage (Test Profile, Test Target)
-- This assumes Foreign Keys will handle cascading or we delete explicitly if needed.
-- Only deleting if they match the garbage names to be safe.
DELETE FROM export_targets WHERE name = 'Insite Cloud' AND profile_id = 1;
DELETE FROM export_profiles WHERE name = 'Test Profile Unique';

-- 3. Restore Mappings for Target 2 ("Insite Cloud (HTTP)")
-- First, ensure no partial duplicates exist for these points on this target.
DELETE FROM export_target_mappings WHERE target_id = 2 AND point_id IN (18, 19, 20, 21, 22);

-- Insert correct mappings based on user screenshot (Building ID 280)
INSERT INTO export_target_mappings (target_id, point_id, target_field_name, building_id, is_enabled) VALUES
(2, 18, 'WLS.PV', 280, 1),
(2, 19, 'WLS.SRS', 280, 1),
(2, 20, 'WLS.SCS', 280, 1),
(2, 21, 'WLS.SSS', 280, 1),
(2, 22, 'WLS.SBV', 280, 1);
