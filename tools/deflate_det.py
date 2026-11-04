# A DEFLATE (RFC 1951) / zlib (RFC 1950) compressor written in plain Python, so its output depends only on its
# input. zlib.compress() does not: the stream differs between zlib builds (1.2.11 and 1.3 pick different match
# ends at level 9 for the same bytes), which is why src/ui/i18n/*.h came out different on every machine even
# when the translation text was identical. Only integer arithmetic on the input bytes is used here, so a given
# table always produces the same stream, and any inflater (lynx::inflateZlib, zlib, JUCE) reads the result.
#
# One dynamic-Huffman block over the whole input; LZ77 with a 32 KiB window, greedy parsing over a short chain of
# candidate matches. Good enough for the i18n tables (a few percent larger than zlib level 9).
import heapq

WINDOW = 1 << 15
MIN_MATCH = 3
MAX_MATCH = 258
CHAIN = 10 # candidate matches tried per position

LENGTH_BASE = [3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163,
               195, 227, 258]
LENGTH_EXTRA = [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0]
DIST_BASE = [1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049,
             3073, 4097, 6145, 8193, 12289, 16385, 24577]
DIST_EXTRA = [0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13]
# the order the code-length alphabet is written in (RFC 1951 3.2.7)
CL_ORDER = [16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15]
NLIT, NDIST, NCL = 286, 30, 19

# length / distance -> (symbol, extra bits, extra value)
LEN_CODE = {}
for _i, _base in enumerate(LENGTH_BASE):
    for _k in range(1 << LENGTH_EXTRA[_i]):
        LEN_CODE[_base + _k] = (257 + _i, LENGTH_EXTRA[_i], _k)
LEN_CODE[258] = (285, 0, 0) # 258 has its own symbol; the 284 encoding of it above is unused
DIST_CODE = {}
for _i, _base in enumerate(DIST_BASE):
    for _k in range(1 << DIST_EXTRA[_i]):
        DIST_CODE[_base + _k] = (_i, DIST_EXTRA[_i], _k)


class _Bits:
    """DEFLATE bit stream: packed LSB-first, Huffman codes written most significant bit first."""

    def __init__(self):
        self.out = bytearray()
        self.acc = 0
        self.count = 0

    def put(self, value, bits):
        self.acc |= value << self.count
        self.count += bits
        while self.count >= 8:
            self.out.append(self.acc & 0xFF)
            self.acc >>= 8
            self.count -= 8

    def put_code(self, code, bits):
        rev = 0
        for k in range(bits):
            rev = (rev << 1) | ((code >> k) & 1)
        self.put(rev, bits)

    def flush(self):
        if self.count:
            self.out.append(self.acc & 0xFF)
            self.acc = 0
            self.count = 0


def _tree_lengths(freqs):
    """Code lengths of an optimal Huffman tree; ties broken by symbol order, so the result is reproducible."""
    n = len(freqs)
    lengths = [0] * n
    heap = [(f, i, i) for i, f in enumerate(freqs) if f > 0]
    heapq.heapify(heap)
    if not heap:
        return lengths
    if len(heap) == 1:
        lengths[heap[0][2]] = 1
        return lengths
    children = {}
    node = n
    while len(heap) > 1:
        f1, _, a = heapq.heappop(heap)
        f2, _, b = heapq.heappop(heap)
        children[node] = (a, b)
        heapq.heappush(heap, (f1 + f2, node, node))
        node += 1
    stack = [(heap[0][2], 0)]
    while stack:
        cur, depth = stack.pop()
        if cur < n:
            lengths[cur] = max(1, depth)
        else:
            a, b = children[cur]
            stack.append((a, depth + 1))
            stack.append((b, depth + 1))
    return lengths


def _huffman_lengths(freqs, limit):
    """Huffman code lengths capped at `limit` bits (zlib refuses deeper tables); flattening the frequencies is
    enough for these inputs and stays deterministic."""
    lengths = []
    for shift in range(32):
        weighted = [max(1, f >> shift) if f else 0 for f in freqs]
        lengths = _tree_lengths(weighted)
        if max(lengths, default=0) <= limit:
            return lengths
    return lengths


def _complete(lengths):
    """Adds codes to unused symbols until the tree is full (Kraft sum 1). zlib rejects an incomplete table, and
    the unused slots cost nothing in the data stream."""
    used = [l for l in lengths if l]
    if not used:
        lengths[0] = 1
        if len(lengths) > 1:
            lengths[1] = 1
        return lengths
    top = max(used)
    need = (1 << top) - sum(1 << (top - l) for l in used)
    free = [i for i, l in enumerate(lengths) if not l]
    while need > 0 and free:
        k = need.bit_length() - 1
        lengths[free.pop()] = top - k
        need -= 1 << k
    return lengths


def _codes(lengths):
    """Canonical code values for the given lengths (RFC 1951 3.2.2)."""
    top = max(lengths, default=0)
    count = [0] * (top + 2)
    for l in lengths:
        if l:
            count[l] += 1
    code = 0
    next_code = [0] * (top + 2)
    for bits in range(1, top + 1):
        code = (code + count[bits - 1]) << 1
        next_code[bits] = code
    out = [0] * len(lengths)
    for i, l in enumerate(lengths):
        if l:
            out[i] = next_code[l]
            next_code[l] += 1
    return out


def _rle_lengths(lengths):
    """Code lengths -> (symbol, extra bits, extra value) with the 16/17/18 repeats."""
    out = []
    n = len(lengths)
    i = 0
    while i < n:
        value = lengths[i]
        run = 1
        while i + run < n and lengths[i + run] == value:
            run += 1
        i += run
        if value == 0:
            while run >= 11:
                take = min(run, 138)
                out.append((18, 7, take - 11))
                run -= take
            while run >= 3:
                take = min(run, 10)
                out.append((17, 3, take - 3))
                run -= take
            out.extend([(0, 0, 0)] * run)
        else:
            out.append((value, 0, 0))
            run -= 1
            while run >= 3:
                take = min(run, 6)
                out.append((16, 2, take - 3))
                run -= take
            out.extend([(value, 0, 0)] * run)
    return out


def _tokens(data):
    """LZ77: literal bytes (int) and (length, distance) matches."""
    n = len(data)
    heads = {}
    out = []
    i = 0
    while i < n:
        length = dist = 0
        if i + MIN_MATCH <= n:
            positions = heads.get(data[i:i + MIN_MATCH])
            if positions:
                longest = min(MAX_MATCH, n - i)
                oldest = i - WINDOW
                tried = 0
                for pos in reversed(positions):
                    if pos < oldest or tried >= CHAIN:
                        break
                    tried += 1
                    l = MIN_MATCH
                    while l + 8 <= longest and data[pos + l:pos + l + 8] == data[i + l:i + l + 8]:
                        l += 8
                    while l < longest and data[pos + l] == data[i + l]:
                        l += 1
                    if l > length:
                        length, dist = l, i - pos
                        if length >= longest:
                            break
        end = i + length if length >= MIN_MATCH else i + 1
        out.append((length, dist) if length >= MIN_MATCH else data[i])
        for p in range(i, end):
            if p + MIN_MATCH <= n:
                heads.setdefault(data[p:p + MIN_MATCH], []).append(p)
        i = end
    return out


def _adler32(data):
    a, b = 1, 0
    for start in range(0, len(data), 4096):
        for c in data[start:start + 4096]:
            a += c
            b += a
        a %= 65521
        b %= 65521
    return (b << 16) | a


def compress(data):
    """`data` -> a zlib stream (header, one dynamic-Huffman block, Adler-32) that any inflater accepts."""
    data = bytes(data)
    tokens = _tokens(data)

    lit_freq = [0] * NLIT
    dist_freq = [0] * NDIST
    for tok in tokens:
        if isinstance(tok, int):
            lit_freq[tok] += 1
        else:
            lit_freq[LEN_CODE[tok[0]][0]] += 1
            dist_freq[DIST_CODE[tok[1]][0]] += 1
    lit_freq[256] += 1 # end of block

    lit_len = _complete(_huffman_lengths(lit_freq, 15))
    dist_len = _complete(_huffman_lengths(dist_freq, 15))
    lit_code = _codes(lit_len)
    dist_code = _codes(dist_len)

    rle = _rle_lengths(lit_len + dist_len)
    cl_freq = [0] * NCL
    for sym, _, _ in rle:
        cl_freq[sym] += 1
    cl_len = _complete(_huffman_lengths(cl_freq, 7))
    cl_code = _codes(cl_len)

    bits = _Bits()
    bits.put(1, 1) # final block
    bits.put(2, 2) # dynamic Huffman
    bits.put(NLIT - 257, 5)
    bits.put(NDIST - 1, 5)
    bits.put(NCL - 4, 4)
    for sym in CL_ORDER:
        bits.put(cl_len[sym], 3)
    for sym, extra_bits, extra in rle:
        bits.put_code(cl_code[sym], cl_len[sym])
        if extra_bits:
            bits.put(extra, extra_bits)
    for tok in tokens:
        if isinstance(tok, int):
            bits.put_code(lit_code[tok], lit_len[tok])
        else:
            sym, extra_bits, extra = LEN_CODE[tok[0]]
            bits.put_code(lit_code[sym], lit_len[sym])
            if extra_bits:
                bits.put(extra, extra_bits)
            sym, extra_bits, extra = DIST_CODE[tok[1]]
            bits.put_code(dist_code[sym], dist_len[sym])
            if extra_bits:
                bits.put(extra, extra_bits)
    bits.put_code(lit_code[256], lit_len[256])
    bits.flush()

    adler = _adler32(data)
    return bytes(bytearray(b"\x78\x01") + bits.out + bytes(adler >> s & 0xFF for s in (24, 16, 8, 0)))
