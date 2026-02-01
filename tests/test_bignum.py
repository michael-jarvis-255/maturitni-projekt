import random
from subprocess import PIPE, Popen
import sys
sys.set_int_max_str_digits(6000)

MAX_NUMBER = 2**8192#2**8192#
TEST_COUNT = 1000

for i in range(TEST_COUNT): # TODO: test different sizes (long a with short b, etc.)
    a,b = [random.randrange(-MAX_NUMBER, MAX_NUMBER) for _ in "ab"]
    print(i, end='\r')
    op = random.choice(['+', '-', '*'])
    p = Popen(["./a.out", str(a), op, str(b)], stdout=PIPE)
    p.wait()
    
    res = int(p.stdout.read().strip())
    expected = {'+': a+b, '-': a-b, '*': a*b}[op]

    if res != expected:
        print()
        print(a, op, b)
        print(f"failed test!\n{expected} expected\n{res} recieved")
        exit(1)
print(' '*64, end='\r')
print(f"all {TEST_COUNT} tests passed")

