class Kolmo < Formula
  desc "Structure-aware lossless compressor for tables, logs and scientific data"
  homepage "https://github.com/adnaneben/kolmo"
  url "https://github.com/adnaneben/kolmo/archive/refs/tags/v0.0.1.tar.gz"
  sha256 "cbc54526fab9e70b134039a7bcaed0ae2c6760c2e46344b035f66d9b8c21894d"
  license "Apache-2.0"
  version_scheme 1
  head "https://github.com/adnaneben/kolmo.git", branch: "main"

  def install
    system "make"
    bin.install "build/kolmo"
  end

  test do
    # round-trip a file and confirm it comes back byte-identical
    (testpath/"in.csv").write (1..500).map { |i| "row,#{i},#{i * 2},ok\n" }.join
    system bin/"kolmo", "-m", "fast", "in.csv", "out.kolmo"
    system bin/"kolmo", "-d", "out.kolmo", "back.csv"
    assert_equal (testpath/"in.csv").read, (testpath/"back.csv").read
  end
end
