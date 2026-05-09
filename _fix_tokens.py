"""
Fix encoding + inject getClinicalWorkstation + fix MainComponent modelRegistry path
"""
import os

def strip_bom_and_normalize(path):
    """Read file, strip BOM if present, return (content_str, had_bom)"""
    with open(path, 'rb') as f:
        raw = f.read()
    had_bom = False
    if raw[:3] == b'\xef\xbb\xbf':
        raw = raw[3:]
        had_bom = True
    elif raw[:2] in (b'\xff\xfe', b'\xfe\xff'):
        text = raw.decode('utf-16')
        return text, True
    return raw.decode('utf-8'), had_bom

def write_utf8_no_bom(path, content):
    """Write content as UTF-8 without BOM"""
    with open(path, 'wb') as f:
        f.write(content.encode('utf-8'))

# ============================================================================
# 1. Fix DesignTokens.h
# ============================================================================
def fix_design_tokens():
    path = r"e:\VIBECode\NerouRuntime\Source\UI\Theme\DesignTokens.h"
    print(f"[1] Fixing {path}...")
    content, had_bom = strip_bom_and_normalize(path)
    if had_bom:
        print("   Stripped BOM")

    lines = content.replace('\r\n', '\n').split('\n')
    
    # Find corruption: statusRunning 0xFF4285F4 followed by waveformBackground 0xFF111316
    start_idx = None
    for i, line in enumerate(lines):
        if '0xFF4285F4' in line and 'statusRunning' in line:
            if i + 1 < len(lines) and '0xFF111316' in lines[i + 1]:
                start_idx = i
                break
    
    if start_idx is not None:
        # Find end: "return cs;" followed by "}"
        end_idx = None
        for i in range(start_idx + 1, len(lines)):
            if lines[i].strip() == 'return cs;':
                for j in range(i + 1, min(i + 5, len(lines))):
                    if lines[j].strip() == '}':
                        end_idx = j
                        break
                break
        
        if end_idx:
            print(f"   Found corrupted block lines {start_idx+1}-{end_idx+1}")
            has_clinical = any('getClinicalWorkstation' in l for l in lines)
            
            repl = [
                '        cs.statusRunning  = juce::Colour(0xFF4285F4);  // Blue 400',
                '        cs.statusIdle     = juce::Colour(0xFF9AA0A6);  // Grey 500',
                '',
                '        return cs;',
                '    }',
            ]
            
            if not has_clinical:
                repl += [
                    '',
                    '    // ClinicalWorkstation - IEC 62366 medical-grade industrial theme',
                    '    inline ColorScheme getClinicalWorkstation(bool isDark = false) {',
                    '        ColorScheme cs;',
                    '        if (!isDark) {',
                    '            cs.primary = juce::Colour(0xFF1976D2); cs.onPrimary = juce::Colour(0xFFFFFFFF);',
                    '            cs.primaryContainer = juce::Colour(0xFFE3F2FD); cs.onPrimaryContainer = juce::Colour(0xFF0D47A1);',
                    '            cs.secondary = juce::Colour(0xFF546E7A); cs.onSecondary = juce::Colour(0xFFFFFFFF);',
                    '            cs.secondaryContainer = juce::Colour(0xFFECEFF1); cs.onSecondaryContainer = juce::Colour(0xFF263238);',
                    '            cs.tertiary = juce::Colour(0xFF5C6BC0); cs.onTertiary = juce::Colour(0xFFFFFFFF);',
                    '            cs.tertiaryContainer = juce::Colour(0xFFE8EAF6); cs.onTertiaryContainer = juce::Colour(0xFF1A237E);',
                    '            cs.error = juce::Colour(0xFFC62828); cs.onError = juce::Colour(0xFFFFFFFF);',
                    '            cs.errorContainer = juce::Colour(0xFFFFEBEE); cs.onErrorContainer = juce::Colour(0xFFB71C1C);',
                    '            cs.surface = juce::Colour(0xFFFFFFFF); cs.onSurface = juce::Colour(0xFF212121);',
                    '            cs.surfaceVariant = juce::Colour(0xFFF5F5F5); cs.onSurfaceVariant = juce::Colour(0xFF616161);',
                    '            cs.surfaceContainer = juce::Colour(0xFFF0F2F5);',
                    '            cs.surfaceContainerHigh = juce::Colour(0xFFE8EAED);',
                    '            cs.surfaceContainerHighest = juce::Colour(0xFFDFE1E5);',
                    '            cs.surfaceContainerLow = juce::Colour(0xFFF5F7FA);',
                    '            cs.surfaceContainerLowest = juce::Colour(0xFFFFFFFF);',
                    '            cs.background = juce::Colour(0xFFF0F2F5); cs.onBackground = juce::Colour(0xFF212121);',
                    '            cs.outline = juce::Colour(0xFFDADCE0); cs.outlineVariant = juce::Colour(0xFFEEEFF1);',
                    '            cs.shadow = juce::Colour(0xFF000000).withAlpha(0.08f);',
                    '            cs.scrim = juce::Colour(0xFF212121).withAlpha(0.50f);',
                    '            cs.waveformBackground = juce::Colour(0xFF1A1C20); cs.waveformGrid = juce::Colour(0xFF2A2E34);',
                    '        } else {',
                    '            cs.primary = juce::Colour(0xFF90CAF9); cs.onPrimary = juce::Colour(0xFF0D47A1);',
                    '            cs.primaryContainer = juce::Colour(0xFF1565C0); cs.onPrimaryContainer = juce::Colour(0xFFBBDEFB);',
                    '            cs.secondary = juce::Colour(0xFF90A4AE); cs.onSecondary = juce::Colour(0xFF263238);',
                    '            cs.secondaryContainer = juce::Colour(0xFF37474F); cs.onSecondaryContainer = juce::Colour(0xFFCFD8DC);',
                    '            cs.tertiary = juce::Colour(0xFF9FA8DA); cs.onTertiary = juce::Colour(0xFF1A237E);',
                    '            cs.tertiaryContainer = juce::Colour(0xFF3949AB); cs.onTertiaryContainer = juce::Colour(0xFFC5CAE9);',
                    '            cs.error = juce::Colour(0xFFEF9A9A); cs.onError = juce::Colour(0xFFB71C1C);',
                    '            cs.errorContainer = juce::Colour(0xFFC62828); cs.onErrorContainer = juce::Colour(0xFFFFCDD2);',
                    '            cs.surface = juce::Colour(0xFF1E2024); cs.onSurface = juce::Colour(0xFFE0E2E6);',
                    '            cs.surfaceVariant = juce::Colour(0xFF2A2D32); cs.onSurfaceVariant = juce::Colour(0xFF9EA2A8);',
                    '            cs.surfaceContainer = juce::Colour(0xFF1A1C20);',
                    '            cs.surfaceContainerHigh = juce::Colour(0xFF252830);',
                    '            cs.surfaceContainerHighest = juce::Colour(0xFF303338);',
                    '            cs.surfaceContainerLow = juce::Colour(0xFF16181C);',
                    '            cs.surfaceContainerLowest = juce::Colour(0xFF111316);',
                    '            cs.background = juce::Colour(0xFF1A1C20); cs.onBackground = juce::Colour(0xFFE0E2E6);',
                    '            cs.outline = juce::Colour(0xFF3C3F44); cs.outlineVariant = juce::Colour(0xFF2A2D32);',
                    '            cs.shadow = juce::Colour(0xFF000000).withAlpha(0.35f);',
                    '            cs.scrim = juce::Colour(0xFF000000).withAlpha(0.80f);',
                    '            cs.waveformBackground = juce::Colour(0xFF111316); cs.waveformGrid = juce::Colour(0xFF1E2228);',
                    '        }',
                    '        cs.waveformChannelColors[0] = juce::Colour(0xFF1976D2);',
                    '        cs.waveformChannelColors[1] = juce::Colour(0xFFE65100);',
                    '        cs.waveformChannelColors[2] = juce::Colour(0xFF2E7D32);',
                    '        cs.waveformChannelColors[3] = juce::Colour(0xFF7B1FA2);',
                    '        cs.waveformChannelColors[4] = juce::Colour(0xFF00838F);',
                    '        cs.waveformChannelColors[5] = juce::Colour(0xFFC62828);',
                    '        cs.waveformChannelColors[6] = juce::Colour(0xFF827717);',
                    '        cs.waveformChannelColors[7] = juce::Colour(0xFFAD1457);',
                    '        cs.waveformActiveChannel = cs.primary;',
                    '        cs.waveformTriggerLine = cs.tertiary;',
                    '        cs.waveformMeasurement = cs.secondary;',
                    '        cs.statusSuccess = juce::Colour(0xFF2E7D32);',
                    '        cs.statusWarning = juce::Colour(0xFFED6C02);',
                    '        cs.statusError = juce::Colour(0xFFC62828);',
                    '        cs.statusInfo = juce::Colour(0xFF1565C0);',
                    '        cs.statusRunning = juce::Colour(0xFF0277BD);',
                    '        cs.statusIdle = juce::Colour(0xFF9E9E9E);',
                    '        return cs;',
                    '    }',
                ]
                print("   Added getClinicalWorkstation()")
            
            lines = lines[:start_idx] + repl + lines[end_idx + 1:]
            print("   Fixed getGoogleMaterial3() ending")
    else:
        has_clinical = any('getClinicalWorkstation' in l for l in lines)
        if has_clinical:
            print("   OK: already clean")
        else:
            print("   WARNING: corruption pattern not found, getClinicalWorkstation missing")
    
    # Fix surfaceVariant indentation
    for i, line in enumerate(lines):
        if '        cs.surfaceVariant' in line and '0xFF2D2F31' in line:
            lines[i] = '            cs.surfaceVariant           = juce::Colour(0xFF2D2F31);'
            print(f"   Fixed surfaceVariant indent at line {i+1}")
    
    write_utf8_no_bom(path, '\r\n'.join(lines))
    print(f"   Written {len(lines)} lines as UTF-8 no-BOM")

# ============================================================================
# 2. Fix MainComponent.cpp - modelRegistryService path
# ============================================================================
def fix_main_component():
    path = r"e:\VIBECode\NerouRuntime\Source\MainComponent.cpp"
    print(f"\n[2] Fixing {path}...")
    content, had_bom = strip_bom_and_normalize(path)
    if had_bom:
        print("   Stripped BOM")
    
    old_line = '    modelRegistryService.setRootDirectory(juce::File::getCurrentWorkingDirectory().getChildFile("onnx"));'
    new_lines = """    // onnx root: derive from bootstrap.savePath (= appRoot/onnx/models)
    {
        juce::File onnxRoot;
        if (bootstrap.savePath.isNotEmpty())
            onnxRoot = juce::File(bootstrap.savePath).getParentDirectory();
        else
            onnxRoot = juce::File::getCurrentWorkingDirectory().getChildFile("onnx");
        modelRegistryService.setRootDirectory(onnxRoot);
    }"""
    
    # Handle both CRLF and LF
    if old_line in content:
        content = content.replace(old_line, new_lines, 1)
        print("   Fixed modelRegistryService.setRootDirectory path")
    else:
        print("   WARNING: modelRegistryService line not found (may already be fixed)")
    
    if had_bom:
        write_utf8_no_bom(path, content)
        print("   Written as UTF-8 no-BOM")
    else:
        with open(path, 'wb') as f:
            f.write(content.encode('utf-8'))
        print("   Written")

# ============================================================================
# 3. Fix build_auto_ci.cmd encoding
# ============================================================================
def fix_build_script():
    path = r"e:\VIBECode\NerouRuntime\build_auto_ci.cmd"
    print(f"\n[3] Fixing {path}...")
    content, had_bom = strip_bom_and_normalize(path)
    if had_bom:
        # Re-save without BOM as pure ASCII
        ascii_content = content.encode('ascii', errors='replace')
        with open(path, 'wb') as f:
            f.write(ascii_content)
        print("   Stripped BOM, saved as ASCII")
    else:
        print("   No BOM detected - file encoding is OK")
        print("   TIP: use _build_now.bat if build_auto_ci.cmd still fails")

if __name__ == '__main__':
    fix_design_tokens()
    fix_main_component()
    fix_build_script()
    print("\n[DONE] Run: cmd /c \"cd /d e:\\VIBECode\\NerouRuntime && _build_now.bat\"")
