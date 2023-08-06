import yaml, random

words = 'the lazy brown fox jumped over the whatever'.split(' ')

def generate(maxDepth, fanout):
    d = {}

    _k = 'a'

    def nextKey():
        nonlocal _k
        if _k[-1] == 'z':
            _k = _k + 'a'
        else:
            _k = _k[:-1] + chr(ord(_k[-1])+1)
        return _k

    def nextScalar(_):
        r = (random.randint(0, 15))
        if r == 0: return True
        if r == 1: return False
        if r>=2 and r<7: return random.randint(0,1000)
        if r>=7 and r<10: return random.randint(0,100) * 10324.
        if r>=10: return random.choice(words)

    def nextDict(lvl):
        out = {}
        fan = (random.randint(0, fanout))
        if lvl < 2: fan = fanout

        for _ in range(fan):
            k = nextKey()
            if lvl == maxDepth:
                v = nextScalar(lvl+1)
            else:
                r = (random.randint(0, 2))
                if r == 0: v = nextDict(lvl+1)
                if r == 1: v = nextList(lvl+1)
                if r == 2: v = nextScalar(lvl+1)
            out[k] = v

        return out

    def nextList(lvl):
        out = []
        fan = (random.randint(0, fanout))
        if lvl < 2: fan = fanout

        for _ in range(fan):
            if lvl == maxDepth:
                v = nextScalar(lvl+1)
            else:
                r = (random.randint(0, 1))
                if r == 0: v = nextList(lvl+1)
                if r == 1: v = nextScalar(lvl+1)
            out.append(v)

        return out

    return nextDict(0)

# print(generate(3, 8))
for i in range(5):
    with open(f'test{i:}.yaml', 'w') as fp:
        d = generate(3,8)
        yaml.dump(d, fp)
