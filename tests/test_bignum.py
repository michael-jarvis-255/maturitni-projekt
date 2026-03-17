import random
from subprocess import PIPE, Popen
import sys
sys.set_int_max_str_digits(6000)

MAX_BITS = 256 # 4096
TEST_COUNT = 1000

def generate_random_number() -> int:
    if (random.random() < 0.2): return 0 # 0 is often a special case we want to test for
    l = random.randrange(0, MAX_BITS)
    return random.randrange(-2**l, 2**l)

def sign(x):
    if x > 0: return 1
    if x == 0: return 0
    return -1



def run(*args):
    try:
        p = Popen(["./a.out", *args], stdout=PIPE, stderr=PIPE)
        p.wait()
    except:
        print()
        print("Error: Popen failed!")
        exit(1)

    res = p.stdout.read().strip()
    if res == b"err": return "err"
    try:
        return int(res)
    except:
        pass

    try:
        return float(res)
    except:
        pass

    print("Error: expected number, received:", res, sep="\n")
    print("args:", args)
    print("stderr:", p.stderr.read().strip())
    exit(1)



def test_binop(test_count, op, solver):
    for i in range(test_count):
        print(f"testing {op}: {i}/{test_count}", end='\r')
        a = generate_random_number()
        b = generate_random_number() if random.random() < 0.2 else a # a==b is often a special case we want to test for

        res = run(op, str(a), str(b))
        correct = solver(a, b)

        if res != correct:
            print(f"Incorrect result!      \nop: {op}\na: {a}\nb: {b}\ngot: {res}\nexp: {correct}")
            exit(1)
    print(f"all tests passed for {op}")

def test_double(test_count):
    for i in range(test_count):
        print(f"testing double: {i}/{test_count}", end='\r')
        x = generate_random_number()

        res = run("double", str(x))
        correct = float(x)
        err = 0.0000000001

        if (correct*(1-err) <= res) != (res <= correct*(1+err)):
            print(f"Incorrect result!      \nop: \"double\"\nx: {x}\n\ngot: {res}\nexp: {correct}")
            exit(1)
    print(f"all tests passed for \"double\"")


TEST_COUNT = 1000
test_binop(TEST_COUNT, "+", lambda x,y: x+y)
test_binop(TEST_COUNT, "-", lambda x,y: x-y)
test_binop(TEST_COUNT, "*", lambda x,y: x*y)
test_binop(TEST_COUNT, "/", lambda x,y: "err" if y==0 else sign(x)*sign(y)*(abs(x)//abs(y)))
test_binop(TEST_COUNT, "%", lambda x,y: "err" if y==0 else x % abs(y))
test_binop(TEST_COUNT, "cmp", lambda x,y: -1 if x < y else 0 if x == y else 1)
test_binop(TEST_COUNT, "xor", lambda x,y: x^y)
test_binop(TEST_COUNT, "and", lambda x,y: x&y)
test_binop(TEST_COUNT, "or", lambda x,y: x|y)
test_double(TEST_COUNT)
