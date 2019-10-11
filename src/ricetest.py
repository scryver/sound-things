def rice_encode(k, val):
    m = 1 << k
    q = val // m
    r = "0b"
    if q:
        r += "1" * q
    r += "0"
    b = bin(val & ((1 << k) - 1))[2:]
    if len(b) < k:
        diff = k - len(b)
        b = "0" * diff + b
    r += b
    return r

# for i in range(18):
#     print(i, rice_encode(1, i))
# for i in range(18):
#     print(i, rice_encode(4, i))

def rice_decode(k, val):
    m = 1 << k
    r = int(val[-k:], 2)
    v = val[2:-k]
    if len(v) > 1:
        assert v[-1] == "0"
        q = len(v[:-1])
        r += q * m
    return r

# for i in range(18):
#     x = rice_encode(1, i)
#     print(i, rice_decode(1, x))
# for i in range(18):
#     x = rice_encode(4, i)
#     print(i, rice_decode(4, x))


def rice_encode2(k, val):
    q = val >> k
    r = "0b"
    if q:
        r += "1" * q
    r += "0"
    if k:
        b = bin(val & ((1 << k) - 1))[2:]
        if len(b) < k:
            diff = k - len(b)
            b = "0" * diff + b
        r += b
    return r

# for i in range(18):
#     print(i, rice_encode2(0, i))
# for i in range(18):
#     print(i, rice_encode2(4, i))

def rice_decode2(k, val):
    v = val[2:]
    if k:
        r = int(v[-k:], 2)
        v = v[:-k]
    else:
        r = 0
    if len(v) > 1:
        assert v[-1] == "0"
        q = len(v[:-1])
        r += q << k
    return r

# for i in range(18):
#     x = rice_encode2(0, i)
#     print(i, rice_decode2(0, x))
# for i in range(18):
#     x = rice_encode2(4, i)
#     print(i, rice_decode2(4, x))

def rice_encode3(k, val):
    q = val >> k
    r = "0b"
    if q:
        r += "1" * q
    # for i in range(q):
    #     r += "1"
    b = bin(val & ((1 << k) - 1))[2:]
    if len(b) < (k + 1):
        diff = (k + 1) - len(b)
        b = "0" * diff + b
    r += b
    return r

def rice_decode3(k, val):
    v = val[2:]
    r = 0
    while v and v[0] == "1":
        r += 1
        v = v[1:]
    assert v[0] == "0"
    v = v[1:]
    r = r << k
    if len(v):
        r += int(v, 2)
    return r

N = 16
ks = [10, 12, 13, 14]
encodings = []
for k in ks:
    encodings.append([rice_encode3(k, (1 << i) + 3*i) for i in range(N)])

print(" {:^40} | {:^40} | {:^40} | {:^40} | {:^40} ".format(0, 1, 2, 4, "bin"))
for i in range(N):
    print(" {:^40} | {:^40} | {:^40} | {:^40} | {:^40} ".format(encodings[0][i],
        encodings[1][i],encodings[2][i],encodings[3][i], bin((1 << i) + 3*i)))

# for k in (0, 1, 2, 4):
#     [print(i, bin(i), rice_encode3(k, i)) for i in range(18)]
#     [print(i, rice_decode3(k, rice_encode3(k, i))) for i in range(18)]
