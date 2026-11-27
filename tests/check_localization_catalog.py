"""Bounded source-level localization guard; not a C++ parser or runtime audit."""
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parents[1]
literal = r'"(?:\\.|[^"\\])*"'
entry = re.compile(r'^\s*\{(' + literal + r'),\s*(' + literal + r'),\s*(' + literal + r')\},', re.M)
lookup = re.compile(
    r'(?:\b(?:menuText|displayText)\s*\(\s*|'
    r'\b(?:textFor|message|status)\s*\(\s*[^,;\n]+,\s*)'
    r'(?:QStringLiteral\(\s*)?((?:' + literal + r'\s*)+)'
)
token = re.compile(literal)
placeholder = re.compile(r'%[1-9][0-9]?')
catalog = root / 'src/ui_language.hpp'
entries = entry.findall(catalog.read_text(encoding='utf-8'))
keys = set()
errors = []
for en, ko, ja in entries:
    if en in keys:
        errors.append('duplicate catalog key: ' + en)
    keys.add(en)
    if any(not value[1:-1].strip() for value in (en, ko, ja)):
        errors.append('blank catalog entry: ' + en)
    if sorted(placeholder.findall(en)) != sorted(placeholder.findall(ko)) or sorted(placeholder.findall(en)) != sorted(placeholder.findall(ja)):
        errors.append('placeholder mismatch: ' + en)
if not entries:
    errors.append('catalog extraction yielded no entries')

# All production GUI source/header files, including new ones in src/gui.
# Explicit lookup literals are required to be cataloged. This intentionally
# does not claim to detect arbitrary unbound widgets or dynamic messages.
gui_files = sorted(set(
    [catalog, root / 'src/app.hpp', root / 'src/main.cpp',
     root / 'src/plot.cpp', root / 'src/report.cpp', root / 'src/navigation.cpp',
     root / 'src/theme.cpp', root / 'src/startup_failure.hpp']
    + list((root / 'src').glob('window*.cpp'))
    + list((root / 'src/gui').rglob('*.cpp'))
    + list((root / 'src/gui').rglob('*.hpp'))
))
count = 0
for path in gui_files:
    source = path.read_text(encoding='utf-8')
    for match in lookup.finditer(source):
        key = '"' + ''.join(v[1:-1] for v in token.findall(match.group(1))) + '"'
        count += 1
        if key not in keys:
            line = source.count('\n', 0, match.start()) + 1
            errors.append(f'{path.relative_to(root)}:{line}: missing explicit localization key {key}')
if count == 0:
    errors.append('lookup extraction yielded no occurrences')
print(f'catalog={len(entries)} unique={len(keys)} explicit_lookups={count} files={len(gui_files)} errors={len(errors)}')
for error in errors:
    print(error, file=sys.stderr)
sys.exit(bool(errors))
