from pathlib import Path

def compare(a, b):
    A = Path(a).read_text().splitlines()
    B = Path(b).read_text().splitlines()
    n = min(len(A), len(B))
    bad = []
    for i in range(n):
        if A[i].split()[1:] != B[i].split()[1:]:
            bad.append((i, A[i], B[i]))
            if len(bad) >= 5:
                break
    print(a, len(A), b, len(B), 'diff', len(bad))
    for x in bad:
        print(x)

compare('build/hashes200.txt', 'build/ref_hashes.txt')
compare('build/video200.txt', 'build/ref_video.txt')
