#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

using Bytes = std::vector<unsigned char>;
struct Header {
    size_t begin, end, code_length;
};

static uint32_t uleb(const Bytes& b, size_t& at, size_t end) {
    uint32_t value = 0;
    for (unsigned shift = 0; shift < 32 && at < end; shift += 7) {
        const unsigned byte = b[at++];
        if (shift == 28 && (byte & 0xf0)) break;
        value |= uint32_t(byte & 0x7f) << shift;
        if (!(byte & 0x80)) return value;
    }
    throw std::runtime_error("unsupported unsigned length");
}

static void locals(const Bytes& b, size_t& at, size_t end) {
    const auto count = uleb(b, at, end);
    for (uint32_t i = 0; i < count; ++i) {
        uleb(b, at, end);
        if (at == end) throw std::runtime_error("missing local type");
        const unsigned type = b[at++];
        // Common one-byte numeric, vector and reference value types.
        if (!((type >= 0x7b && type <= 0x7f) || (type >= 0x69 && type <= 0x70)))
            throw std::runtime_error("unsupported local type");
    }
}

static void append(Bytes& to, const Bytes& from, size_t first, size_t last) {
    to.insert(to.end(), from.begin() + first, from.begin() + last);
}

static Bytes group_headers(const Bytes& input, bool reverse) {
    const unsigned char marker[] = {'K','W','S','1'};
    const unsigned char magic[] = {0, 'a', 's', 'm', 1, 0, 0, 0};
    size_t initial = reverse ? sizeof(marker) : 0;
    if (input.size() < initial + sizeof(magic) ||
        (reverse && std::memcmp(input.data(), marker, sizeof(marker))) ||
        std::memcmp(input.data() + initial, magic, sizeof(magic)))
        throw std::runtime_error("expected a core WebAssembly v1 module");
    Bytes output;
    output.reserve(input.size() + sizeof(marker));
    if (!reverse) output.insert(output.end(), marker, marker + sizeof(marker));
    size_t at = initial + sizeof(magic);
    size_t copied = initial;
    while (at < input.size()) {
        unsigned id = input[at++];
        size_t length = uleb(input, at, input.size());
        if (length > input.size() - at) throw std::runtime_error("incomplete section");
        size_t end = at + length;
        if (id != 10) {
            at = end;
            continue;
        }
        const auto count = uleb(input, at, end);
        append(output, input, copied, at);
        if (count > (end - at) / 2) throw std::runtime_error("invalid function count");
        std::vector<Header> headers;
        headers.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            size_t first = at;
            size_t body = uleb(input, at, end);
            size_t body_start = at;
            if (body > end - at) throw std::runtime_error("invalid function body length");
            locals(input, at, reverse ? end : body_start + body);
            size_t local_bytes = at - body_start;
            if (body < local_bytes) throw std::runtime_error("inconsistent locals");
            size_t code_length = body - local_bytes;
            headers.push_back({first, at, code_length});
            if (!reverse) at += code_length;
        }
        if (reverse) {
            for (auto h : headers) {
                if (h.code_length > end - at) throw std::runtime_error("incomplete body");
                append(output, input, h.begin, h.end);
                append(output, input, at, at + h.code_length);
                at += h.code_length;
            }
        } else {
            for (auto h : headers) append(output, input, h.begin, h.end);
            for (auto h : headers) append(output, input, h.end, h.end + h.code_length);
        }
        if (at != end) throw std::runtime_error("inconsistent code section");
        copied = end;
    }
    append(output, input, copied, input.size());
    return output;
}

int main(int argc, char** argv) {
    if (argc != 4 || (std::strcmp(argv[1], "group") && std::strcmp(argv[1], "restore")))
        return 2;
    try {
        std::ifstream in(argv[2], std::ios::binary);
        if (!in) throw std::runtime_error("cannot open input");
        in.seekg(0, std::ios::end);
        auto bytes = in.tellg();
        if (bytes < 0) throw std::runtime_error("unknown input size");
        in.seekg(0);
        Bytes input(static_cast<size_t>(bytes));
        if (!in.read(reinterpret_cast<char*>(input.data()), bytes))
            throw std::runtime_error("cannot read input");
        auto result = group_headers(input, !std::strcmp(argv[1], "restore"));
        std::ofstream out(argv[3], std::ios::binary);
        out.write(reinterpret_cast<const char*>(result.data()), result.size());
        out.close();
        if (!out) throw std::runtime_error("cannot write output");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
