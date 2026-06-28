import os
import sys
import subprocess
import glob

def run_tests():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    
    # Auto-detect binary
    binary_path = None
    if len(sys.argv) >= 2:
        binary_path = sys.argv[1]
    else:
        candidates = [
            os.path.join(script_dir, "..", "build", "Release", "chkoupi.exe"),
            os.path.join(script_dir, "..", "build", "chkoupi"),
            os.path.join(script_dir, "..", "build", "Debug", "chkoupi.exe"),
            os.path.join(script_dir, "..", "build", "chkoupi.exe")
        ]
        for candidate in candidates:
            if os.path.exists(candidate):
                binary_path = candidate
                break
                
    if not binary_path:
        print("ERROR: Cannot find chkoupi binary. Build first or pass path as argument.")
        sys.exit(1)
        
    print(f"Using: {binary_path}")
    print("========================================")
    
    passed = 0
    failed = 0
    errors = []
    
    # ── Pass tests (expected stdout) ─────────────────────────────────────────────
    expected_files = glob.glob(os.path.join(script_dir, "*.expected"))
    for expected_path in sorted(expected_files):
        test_name = os.path.basename(expected_path)[:-9]
        dz_path = os.path.join(script_dir, f"{test_name}.dz")
        if not os.path.exists(dz_path):
            continue
            
        # Run compiler
        try:
            res = subprocess.run([binary_path, dz_path], capture_output=True, text=True, timeout=5)
            if res.returncode != 0:
                print(f"FAIL  {test_name}  (compiler exited with code {res.returncode})")
                print(res.stderr)
                failed += 1
                errors.append(f"{test_name}: compiler exited with error code {res.returncode}")
                continue
                
            actual = res.stdout.replace('\r\n', '\n').strip()
            with open(expected_path, 'r', encoding='utf-8') as f:
                expected_content = f.read().replace('\r\n', '\n').strip()
                
            if actual == expected_content:
                print(f"PASS  {test_name}")
                passed += 1
            else:
                print(f"FAIL  {test_name}")
                failed += 1
                errors.append(f"{test_name}:\n  expected:\n{expected_content}\n  got:\n{actual}")
        except Exception as ex:
            print(f"FAIL  {test_name}  (compiler crashed or timed out: {ex})")
            failed += 1
            errors.append(f"{test_name}: compiler crashed/timed out: {ex}")
            
    # ── Error tests (expected compiler error) ────────────────────────────────────
    expected_err_files = glob.glob(os.path.join(script_dir, "*.expected_err"))
    for expected_err_path in sorted(expected_err_files):
        test_name = os.path.basename(expected_err_path)[:-13]
        dz_path = os.path.join(script_dir, f"{test_name}.dz")
        if not os.path.exists(dz_path):
            continue
            
        try:
            res = subprocess.run([binary_path, dz_path], capture_output=True, text=True, timeout=5)
            if res.returncode == 0:
                print(f"FAIL  {test_name}  (should have failed but succeeded)")
                failed += 1
                errors.append(f"{test_name}: expected error but compiler succeeded")
                continue
                
            actual_err = res.stderr.replace('\r\n', '\n').strip()
            with open(expected_err_path, 'r', encoding='utf-8') as f:
                expected_msg = f.read().replace('\r\n', '\n').strip()
                
            if expected_msg in actual_err:
                print(f"PASS  {test_name}")
                passed += 1
            else:
                print(f"FAIL  {test_name}  (wrong error message)")
                failed += 1
                errors.append(f"{test_name}:\n  expected error containing: {expected_msg}\n  got:\n{actual_err}")
        except Exception as ex:
            print(f"FAIL  {test_name}  (crashed/timed out: {ex})")
            failed += 1
            errors.append(f"{test_name}: crashed/timed out: {ex}")
            
    # ── Summary ──────────────────────────────────────────────────────────────────
    print("========================================")
    total = passed + failed
    print(f"Results: {passed}/{total} passed")
    
    if failed > 0:
        print("\nFailures:")
        for err in errors:
            print(err)
            print("-" * 40)
        sys.exit(1)
    else:
        print("All tests passed!")
        sys.exit(0)

if __name__ == '__main__':
    run_tests()
