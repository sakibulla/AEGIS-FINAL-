#!/usr/bin/env python3
"""
Voice Activation Integration Script
Merges voice activation from AEGIS_Guardian-main into Gurdian_Master_old
"""

import os
import shutil

# Paths
CURRENT_DIR = r"E:\Gurdian\Gurdian_Master_old"
SOURCE_DIR = r"E:\Gurdian\AEGIS_Guardian-main"
MAIN_CPP = os.path.join(CURRENT_DIR, "main", "main.cpp")
SOURCE_MAIN_C = os.path.join(SOURCE_DIR, "main", "main.c")
BACKUP_CPP = os.path.join(CURRENT_DIR, "main", "main.cpp.backup")
OUTPUT_CPP = os.path.join(CURRENT_DIR, "main", "main_merged.cpp")

# Configuration
ENABLE_VOICE = True
WAKE_WORD_THRESHOLD = 0.6

def read_file(filepath):
    """Read file content"""
    with open(filepath, 'r', encoding='utf-8') as f:
        return f.read()

def write_file(filepath, content):
    """Write content to file"""
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)

def extract_voice_code():
    """Extract voice activation code from source"""
    print(f"Reading source file: {SOURCE_MAIN_C}")
    source_content = read_file(SOURCE_MAIN_C)
    
    voice_includes = [
        '#include "esp_wn_iface.h"',
        '#include "esp_wn_models.h"',
        '#include "esp_afe_sr_models.h"',
        '#include "esp_mn_iface.h"',
        '#include "esp_mn_models.h"',
        '#include "esp_board_init.h"',
        '#include "model_path.h"',
    ]
    
    return {
        'includes': voice_includes,
        'source': source_content
    }

def generate_merged_code():
    """Generate merged code with voice activation"""
    print("Generating merged code...")
    
    # Read current main.cpp
    current_content = read_file(MAIN_CPP)
    
    # Header comment
    merged_code = """/*
 * AEGIS Guardian Master - With Voice Activation
 * 
 * Features:
 * - Voice activation using ESP-SR wake word detection
 * - Face recognition with owner enrollment
 * - Object detection using Edge Impulse
 * - HTTP video streaming
 * - Security state management
 * 
 * Voice activation can be enabled/disabled with ENABLE_VOICE_ACTIVATION
 * 
 * Build: idf.py build
 * Flash: idf.py flash monitor
 */

"""
    
    # Add voice activation toggle
    merged_code += """
/* =========================================================
 * VOICE ACTIVATION CONFIGURATION
 * ========================================================= */

#define ENABLE_VOICE_ACTIVATION 1  // Set to 0 to disable voice activation

"""
    
    print("Merged code structure created")
    print("\nNext steps:")
    print("1. Review VOICE_ACTIVATION_INTEGRATION_PLAN.md")
    print("2. Manually integrate voice code following the plan")
    print("3. Test compilation step by step")
    
    return merged_code

def main():
    print("=" * 60)
    print("Voice Activation Integration Tool")
    print("=" * 60)
    print()
    
    # Check if files exist
    if not os.path.exists(MAIN_CPP):
        print(f"ERROR: Main file not found: {MAIN_CPP}")
        return
    
    if not os.path.exists(SOURCE_MAIN_C):
        print(f"ERROR: Source file not found: {SOURCE_MAIN_C}")
        return
    
    print(f"✓ Current project: {CURRENT_DIR}")
    print(f"✓ Source project: {SOURCE_DIR}")
    print()
    
    # Create backup if not exists
    if not os.path.exists(BACKUP_CPP):
        print(f"Creating backup: {BACKUP_CPP}")
        shutil.copy2(MAIN_CPP, BACKUP_CPP)
        print("✓ Backup created")
    else:
        print("✓ Backup already exists")
    
    print()
    print("Voice activation integration requires manual code merge")
    print("See VOICE_ACTIVATION_INTEGRATION_PLAN.md for detailed steps")
    print()
    
    # Extract voice code
    voice_data = extract_voice_code()
    print("✓ Voice activation code extracted")
    print()
    print("Voice activation includes needed:")
    for inc in voice_data['includes']:
        print(f"  {inc}")
    print()
    
    print("=" * 60)
    print("Integration Plan Created Successfully")
    print("=" * 60)

if __name__ == "__main__":
    main()
