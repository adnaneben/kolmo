class Kolmo < Formula
  desc "Structure-aware lossless compressor for tables, logs and scientific data"
  homepage "https://github.com/adnaneben/kolmo"
  url "https://github.com/adnaneben/kolmo/archive/refs/tags/v0.0.2.tar.gz"
  sha256 "fab2a448988068c6755755af56415bb92de4f52c14f02ef10d52cd7781c12fec"
  license "Apache-2.0"
  version_scheme 1
  head "https://github.com/adnaneben/kolmo.git", branch: "main"

  def install
    system "make"
    bin.install "build/kolmo"
  end

  test do
    # round-trip a file and confirm it comes back byte-identical
    rows = (1..500).map { |i| [i, i * 2] }
    (testpath/"in.csv").write rows.map { |a, b| "row,#{a},#{b},ok\n" }.join
    system bin/"kolmo", "-9", "in.csv"
    assert_path_exists testpath/"in.csv.kolmo"
    system bin/"kolmo", "-d", "in.csv.kolmo", "-o", "back.csv"
    assert_equal (testpath/"in.csv").read, (testpath/"back.csv").read
    # standard input to standard output, level 1
    packed = pipe_output("#{bin}/kolmo -1", (testpath/"in.csv").read)
    assert_equal (testpath/"in.csv").read, pipe_output("#{bin}/kolmo -dc", packed)
  end
end
