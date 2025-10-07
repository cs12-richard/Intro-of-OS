import random
n = 100000
random.seed(0)
# create list and shuffle
arr = list(range(1, n+1))
random.shuffle(arr)
with open('input.txt', 'w') as f:
    f.write(str(n) + '\n')
    # write in chunks to avoid huge temporary strings
    CHUNK = 10000
    for i in range(0, n, CHUNK):
        f.write(' '.join(map(str, arr[i:i+CHUNK])))
        if i + CHUNK < n:
            f.write(' ')
print('Wrote input.txt with', n, 'numbers')
