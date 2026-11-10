class Kolmo < Formula
  desc "Structure-aware lossless compressor for tables, logs and scientific data"
  homepage "https://github.com/adnaneben/kolmo"
  url "https://github.com/adnaneben/kolmo/archive/refs/tags/v1.0.1.tar.gz"
  sha256 "cf2a59aaacc1232b2c711a638302104308e77a54c32b476db9cfa33e722d1430"
  license "Apache-2.0"
  head "https://github.com/adnaneben/kolmo.git", branch: "main"

  depends_on "xz" # liblzma, for the max/ultra tiers

  def install
    system "make"
    bin.install "build/kolmo"
  end

  test do
    # round-trip a file and confirm it comes back byte-identical
    (testpath/"in.csv").write (1..500).map { |i| "row,#{i},#{i * 2},ok\n" }.join
    system bin/"kolmo", "-m", "fast", "in.csv", "out.czn"
    system bin/"kolmo", "-d", "out.czn", "back.csv"
    assert_equal (testpath/"in.csv").read, (testpath/"back.csv").read
  end
end
