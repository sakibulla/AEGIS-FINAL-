#!/usr/bin/env python3
"""
Automatically applies voice activation to main.cpp
Creates a backup before modifying
"""

import os
import re
import shutil
from datetime import datetime

MAIN_CPP = r"E:\Gurdian\Gurdian_Master_old\main\main.cpp"
BACKUP_DIR = r"E:\Gurdian\Gurdian_Master_old\backups"

def create_backup():
    """Create timestamped backup"""
    os.makedirs(BACKUP_DIR, exist_ok=True)
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    backup_path = os.path.join(BACKUP_DIR, f"main.cpp.{timestamp}")
    shutil.copy2(MAIN_CPP, backup_path)
    print(f"✓ Backup created: {backup_path}")
    return backup_path

def read_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        return f.read()

def write_file(filepath, content):
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)

def apply_voice_activation():
    """Apply voice activation modifications to main.cpp"""
    
    print("=" * 70)
    print("Voice Activation Integration Script")
    print("=" * 70)
    print()
    
    # Create backup
    backup_path = create_backup()
    
    # Read current content
    print("✓ Reading main.cpp...")
    content = read_file(MAIN_CPP)
    
    # Check if already modified
    if 'ENABLE_VOICE_ACTIVATION' in content:
        print()
        print("⚠ Voice activation already integrated!")
        print("  To reapply, restore from backup first")
        return
    
    # Step 1: Add voice activation toggle after includes
    print("✓ Adding voice activation toggle...")
    
    include_section_end = content.find('static const char *TAG = "AEGIS";')
    if include_section_end == -1:
        print("✗ Could not find TAG definition")
        return
    
    voice_toggle = '''
/* =========================================================
 * VOICE ACTIVATION CONFIGURATION
 * ========================================================= */

#define ENABLE_VOICE_ACTIVATION 1  // Set to 0 to disable

#if ENABLE_VOICE_ACTIVATION
#include "voice_activation.h"
#endif

'''
    
    content = content[:include_section_end] + voice_toggle + content[include_section_end:]
    
    # Step 2: Modify app_main function
    print("✓ Modifying app_main() function...")
    
    # Find app_main function
    app_main_start = content.find('extern "C" void app_main(void)')
    if app_main_start == -1:
        print("✗ Could not find app_main function")
        return
    
    # Find where camera init happens
    camera_init_pos = content.find('if (init_camera()', app_main_start)
    if camera_init_pos == -1:
        print("✗ Could not find camera initialization")
        return
    
    # Find the start of camera initialization section
    camera_section_start = content.rfind('/* =====', 0, camera_init_pos)
    
    # Insert voice activation code before camera init
    voice_activation_code = '''
#if ENABLE_VOICE_ACTIVATION
    /* =====================================================
     * VOICE ACTIVATION MODE
     * ===================================================== */
    
    ESP_LOGI(TAG, "Voice activation: ENABLED");
    
    // Initialize voice recognition BEFORE camera
    if (voice_activation_init() == ESP_OK) {
        // Start voice recognition tasks
        voice_activation_start();
        
        ESP_LOGI(TAG, "====================================");
        ESP_LOGI(TAG, "  Waiting for wake word...");
        ESP_LOGI(TAG, "  Say 'Hi ESP' to activate camera");
        ESP_LOGI(TAG, "====================================");
        
        // Wait for wake word (blocks until detected)
        voice_activation_wait(portMAX_DELAY);
        
        ESP_LOGI(TAG, "Wake word detected! Activating system...");
    } else {
        ESP_LOGW(TAG, "Voice init failed, using normal mode");
    }
#else
    ESP_LOGI(TAG, "Voice activation: DISABLED (normal mode)");
#endif

    '''
    
    content = content[:camera_section_start] + voice_activation_code + content[camera_section_start:]
    
    # Step 3: Add voice status to final log
    print("✓ Adding voice status to boot messages...")
    
    ready_section = content.find('ESP_LOGI(\n        TAG,\n        "          AEGIS READY");')
    if ready_section > 0:
        # Find the end of AEGIS READY section
        after_ready = content.find('ESP_LOGI(\n        TAG,\n        "Camera: QVGA 320x240");', ready_section)
        if after_ready > 0:
            voice_status = '''
#if ENABLE_VOICE_ACTIVATION
    ESP_LOGI(TAG, "Mode: VOICE ACTIVATED");
    ESP_LOGI(TAG, "Wake word: Hi ESP");
#else
    ESP_LOGI(TAG, "Mode: ALWAYS ON");
#endif

    '''
            content = content[:after_ready] + voice_status + content[after_ready:]
    
    # Write modified content
    print("✓ Writing modified main.cpp...")
    write_file(MAIN_CPP, content)
    
    print()
    print("=" * 70)
    print("✓ Voice activation integrated successfully!")
    print("=" * 70)
    print()
    print("Next steps:")
    print("  1. Review the changes in main.cpp")
    print("  2. Build: idf.py build")
    print("  3. Flash: idf.py flash monitor")
    print()
    print("To disable voice activation:")
    print("  Set ENABLE_VOICE_ACTIVATION to 0 in main.cpp")
    print()
    print(f"To restore original: copy {backup_path} to main.cpp")
    print()

if __name__ == "__main__":
    try:
        apply_voice_activation()
    except Exception as e:
        print(f"\n✗ Error: {e}")
        print("\nRestore from backup if needed")
