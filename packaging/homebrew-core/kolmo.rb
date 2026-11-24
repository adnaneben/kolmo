# homebrew-core draft (Formula/k/kolmo.rb). Unlike the tap formula (packaging/homebrew/kolmo.rb) it has no
# version_scheme, builds for the baseline CPU, and installs the library and header too. homebrew-core adds the
# bottle block itself. Not eligible before 225 stars, 90 forks or 90 watchers (self-submission rule):
# packaging/README.md.
class Kolmo < Formula
  desc "Structure-aware lossless compressor for tables, logs and scientific data"
  homepage "https://github.com/adnaneben/kolmo"
  url "https://github.com/adnaneben/kolmo/archive/refs/tags/v0.0.2.tar.gz"
  sha256 "fab2a448988068c6755755af56415bb92de4f52c14f02ef10d52cd7781c12fec"
  license all_of: [
    "Apache-2.0",
    "Apache-2.0" => { with: "LLVM-exception" },
  ]
  head "https://github.com/adnaneben/kolmo.git", branch: "main"

  def install
    # ARCH= drops the Makefile's -march=native/-mcpu=native default, so bottles run on every CPU of the
    # architecture. The Makefile has no install target, and no soname or install name for the library.
    if OS.mac?
      gc = "-Wl,-dead_strip"
      soname = "-Wl,-install_name,#{opt_lib}/libkolmo.0.dylib"
    else
      gc = "-Wl,--gc-sections"
      soname = "-Wl,-soname,libkolmo.so.0"
    end
    system "make", "ARCH=", "LIBS=#{gc} -lpthread -lm", "build/kolmo"
    system "make", "ARCH=", "LIBS=#{soname} #{gc} -lpthread -lm", "lib"

    bin.install "build/kolmo"
    include.install "src/kolmo.h"
    if OS.mac?
      lib.install "build/libkolmo.dylib" => "libkolmo.0.dylib"
      lib.install_symlink "libkolmo.0.dylib" => "libkolmo.dylib"
    else
      lib.install "build/libkolmo.so" => "libkolmo.so.0"
      lib.install_symlink "libkolmo.so.0" => "libkolmo.so"
    end
  end

  test do
    # the command line: round trip by file at level 9, then through a pipe at level 1
    rows = (1..500).map { |i| [i, i * 2] }
    (testpath/"in.csv").write rows.map { |a, b| "row,#{a},#{b},ok\n" }.join
    system bin/"kolmo", "-9", "in.csv"
    assert_path_exists testpath/"in.csv.kolmo"
    system bin/"kolmo", "-d", "in.csv.kolmo", "-o", "back.csv"
    assert_equal (testpath/"in.csv").read, (testpath/"back.csv").read
    packed = pipe_output("#{bin}/kolmo -1", (testpath/"in.csv").read)
    assert_equal (testpath/"in.csv").read, pipe_output("#{bin}/kolmo -dc", packed)

    # the library: compress and decompress a buffer through the C API
    (testpath/"test.c").write <<~C
      #include <kolmo.h>
      #include <string.h>
      int main(void) {
        static uint8_t src[4096], dst[65536], back[4096];
        for (int i = 0; i < 4096; i++) src[i] = (uint8_t)"row,1,2,ok\\n"[i % 11];
        kolmo_params p;
        memset(&p, 0, sizeof p);
        if (kolmo_params_for_level(3, &p) != 0) return 1;
        int64_t c = kolmo_compress(src, sizeof src, dst, sizeof dst, &p);
        if (c <= 0) return 2;
        if (kolmo_decompress(dst, (size_t)c, back, sizeof back, 1) != (int64_t)sizeof src) return 3;
        return memcmp(src, back, sizeof src) != 0;
      }
    C
    system ENV.cc, "test.c", "-I#{include}", "-L#{lib}", "-lkolmo", "-o", "test"
    system "./test"
  end
end
