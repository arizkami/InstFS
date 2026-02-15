#!/usr/bin/env python3
"""
Verify that sample files referenced in JSON exist
"""

import json
import os
import sys

def verify_json_samples(json_path):
    """Check if all sample files in JSON exist"""
    
    with open(json_path, 'r') as f:
        data = json.load(f)
    
    base_dir = os.path.dirname(json_path)
    regions = data.get('regions', [])
    
    print(f"Checking {len(regions)} regions...")
    
    unique_samples = set()
    missing_samples = []
    
    for i, region in enumerate(regions):
        sample = region.get('sample')
        if not sample:
            print(f"  Region {i}: No sample specified")
            continue
        
        unique_samples.add(sample)
        
        # Build full path
        full_path = os.path.join(base_dir, sample)
        
        if not os.path.exists(full_path):
            missing_samples.append(sample)
            if len(missing_samples) <= 5:  # Only print first 5
                print(f"  Missing: {sample}")
    
    print(f"\nSummary:")
    print(f"  Total regions: {len(regions)}")
    print(f"  Unique samples: {len(unique_samples)}")
    print(f"  Missing samples: {len(missing_samples)}")
    
    if missing_samples:
        print(f"\nWarning: {len(missing_samples)} sample files are missing!")
        return False
    else:
        print(f"\nAll sample files exist!")
        return True

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <json_file>")
        sys.exit(1)
    
    json_file = sys.argv[1]
    
    if not os.path.exists(json_file):
        print(f"Error: JSON file not found: {json_file}")
        sys.exit(1)
    
    success = verify_json_samples(json_file)
    sys.exit(0 if success else 1)
