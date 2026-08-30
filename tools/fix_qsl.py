import re

BS = chr(92)  # backslash

def fix_file(path):
    s = open(path, encoding='utf-8').read()
    out = []
    i = 0
    n = len(s)
    fixed = 0
    while True:
        j = s.find('QStringLiteral(', i)
        if j < 0:
            out.append(s[i:])
            break
        out.append(s[i:j])
        k = j + len('QStringLiteral(')
        depth = 1
        inq = False
        while k < n and depth > 0:
            c = s[k]
            if inq:
                if c == BS:
                    k += 2
                    continue
                if c == '"':
                    inq = False
            else:
                if c == '"':
                    inq = True
                elif c == '(':
                    depth += 1
                elif c == ')':
                    depth -= 1
            k += 1
        content = s[j + len('QStringLiteral('):k - 1]
        if '" + Theme::' not in content:
            out.append(s[j:k])
            i = k
            continue
        toks = re.findall(r'"(?:[^"\\]|\\.)*"|\+|Theme::\w+|\s+|.', content)
        pieces = []
        lit = []
        bad = False
        for t in toks:
            if t.startswith('"'):
                lit.append(t)
            elif t.strip() == '+' or t.strip() == '':
                continue
            elif re.fullmatch(r'Theme::\w+', t):
                if lit:
                    pieces.append(('L', ''.join(lit)))
                    lit = []
                pieces.append(('T', t))
            else:
                bad = True
                break
        if bad:
            out.append(s[j:k])
            i = k
            continue
        if lit:
            pieces.append(('L', ''.join(lit)))
        expr = ' + '.join(('QStringLiteral(%s)' % p[1]) if p[0] == 'L' else p[1] for p in pieces)
        out.append(expr)
        fixed += 1
        i = k
    open(path, 'w', encoding='utf-8', newline='').write(''.join(out))
    return fixed

if __name__ == '__main__':
    print('MainWindow.cpp blocks:', fix_file('src/MainWindow.cpp'))
