
import sqlite3
import json
import sys

DB_PATH = '/Users/kyungho/Project/PulseOne/data/db/pulseone.db'
TARGET_IDS = [10, 18]

def fix_auth_headers():
    try:
        conn = sqlite3.connect(DB_PATH)
        cursor = conn.cursor()

        for target_id in TARGET_IDS:
            cursor.execute("SELECT config FROM export_targets WHERE id=?", (target_id,))
            row = cursor.fetchone()

            if not row:
                print(f"Target ID {target_id} not found.")
                continue

            config_wrapper = json.loads(row[0])
            
            # The config is an array of objects
            modified = False
            for config in config_wrapper:
                if 'headers' in config:
                    headers = config['headers']
                    if 'Authorization' in headers:
                        print(f"Removing Authorization header from Target {target_id}...")
                        del headers['Authorization']
                        modified = True
                    if 'x-api-key' in headers:
                        print(f"Removing x-api-key header from Target {target_id} (handled by auth block)...")
                        del headers['x-api-key']
                        modified = True
            
            if modified:
                new_config_json = json.dumps(config_wrapper)
                cursor.execute("UPDATE export_targets SET config=? WHERE id=?", (new_config_json, target_id))
                print(f"Updated Target {target_id}.")
            else:
                print(f"Target {target_id} - No changes needed.")

        conn.commit()
        conn.close()
        print("Fix applied successfully.")
    
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    fix_auth_headers()
