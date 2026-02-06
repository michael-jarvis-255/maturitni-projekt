import random
from subprocess import PIPE, Popen
import sys
sys.set_int_max_str_digits(6000)

MAX_BITS = 4096
TEST_COUNT = 1000

def sign(x):
    if x > 0: return 1
    if x == 0: return 0
    return -1

for i in range(TEST_COUNT): # TODO: test different sizes (long a with short b, etc.)
    al,bl = [random.randrange(0, MAX_BITS) for _ in "ab"]
    a,b = [random.randrange(-2**l, 2**l) for l in (al, bl)]
    print(i, end='\r')
    op = random.choice(['+', '-', '*', '/'])

    try:
        p = Popen(["./a.out", str(a), op, str(b)], stdout=PIPE)
        p.wait()
    except:
        print()
        print(a, op, b)
        print("failed test! received error")
        exit(1)
    
    res = int(p.stdout.read().strip())
    expected = {
        '+': a+b,
        '-': a-b,
        '*': a*b,
        '/': (abs(a)//abs(b)) * sign(a)*sign(b)
    }[op]

    if res != expected:
        print()
        print(a, op, b)
        print(f"failed test!\n{expected} expected\n{res} recieved")
        exit(1)
print(' '*64, end='\r')
print(f"all {TEST_COUNT} tests passed")

